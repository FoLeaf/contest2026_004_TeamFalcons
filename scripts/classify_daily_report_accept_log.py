#!/usr/bin/env python3
"""Classify a captured VelaGuard daily-report acceptance trace.

The classifier is deliberately offline and side-effect free.  The serial
collector preserves the complete console, then this program decides whether
one board-driven round asked for today's report, wrote the exact path, and
ended successfully before the caller restored the held file.
"""

from __future__ import annotations

import argparse
import datetime as dt
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Sequence


MARKERS = {
    "HOLD": r"^HOLD OK hold=(\S+)$",
    "REMOVED": r"^REMOVED OK path=(\S+)$",
    "REQUEST": r"^vghmi: daily report requested$",
    "SUBMITTED": r"^\[vgagent\] round submitted: (?:1[0-9][0-9]|[2-9][0-9]|[1-9][0-9]{2,}) bytes$",
    "TRACE_BEGIN": r"^\[trace:([0-9a-f]{16})\] BEGIN chat=(\S+) chan=local_client$",
    "WRITE_INTENT": r"^\[agent\] Tool call: write_file args=(\{.*\})$",
    "WRITE_EXEC": r"^\[tools\] Executing tool: write_file$",
    "TRACE_END": r"^\[trace:([0-9a-f]{16})\] END status=(\S+) iters=(\d+) tools=(\d+)(?: .*)?$",
    "VERIFIED": r"^VERIFY OK path=(\S+)(?: bytes=(\d+))?$",
    "RESTORED": r"^RESTORE OK path=(\S+) hold=(\S+) cmp=identical$",
    "CLEANUP": r"^CLEANUP OK hold=(\S+)$",
}

REPORT_DATE_RE = re.compile(r"^daily-(\d{4}-\d{2}-\d{2})\.md$")
REPORT_PATH_RE = re.compile(r"^/data/velaguard/reports/daily-(\d{4}-\d{2}-\d{2})\.md$")
REPORT_DIR_RE = re.compile(r"^/data/velaguard/reports/daily-\d{4}-\d{2}-\d{2}_acceptance\.hold\.md$")
EXPECTED_SECTIONS = ("通信质量", "点位在线", "异常时间线")


@dataclass(frozen=True)
class Event:
    index: int
    kind: str
    match: re.Match[str]


@dataclass(frozen=True)
class ReportBody:
    text: str
    date: str
    content: str


@dataclass(frozen=True)
class Result:
    ok: bool
    reason: str
    evidence: str


def split_lines(text: str) -> list[str]:
    """Strip serial framing while preserving meaningful trace separators."""
    # ANSI cursor controls appear after an NSH prompt on the ST-LINK VCP.
    text = re.sub(r"\x1b\[[0-9;?]*[ -/]*[@-~]", "", text)
    text = text.replace("\r", "")
    lines = []
    for line in text.split("\n"):
        # Board-sided commands and background logs often share one physical
        # VCP line.  Remove only the prompt prefix; keep a glued event line.
        line = re.sub(r"^(?:(?:nsh|vela)> ?)+", "", line)
        lines.append(line)
    return lines


def _events(lines: Sequence[str]) -> Iterable[Event]:
    for index, line in enumerate(lines):
        for kind, pattern in MARKERS.items():
            match = re.fullmatch(pattern, line)
            if match:
                yield Event(index, kind, match)


def parse_write_intent(raw_json: str) -> tuple[str | None, str | None]:
    """Return (path, content); malformed JSON is treated as no evidence."""
    import json

    try:
        payload = json.loads(raw_json)
    except (json.JSONDecodeError, TypeError):
        return None, None
    if not isinstance(payload, dict):
        return None, None
    path = payload.get("path")
    content = payload.get("content")
    if not isinstance(path, str) or not isinstance(content, str):
        return None, None
    return path, content


def validate_report(text: str, expected_date: str) -> ReportBody:
    """Mirror the durable parts of vg_ai_report_validate plus required sections."""
    normalized = text.replace("\r\n", "\n").replace("\r", "\n")
    if not normalized or len(normalized.encode("utf-8")) > 1400:
        raise ValueError("report is empty or exceeds 1400 UTF-8 bytes")
    if "\x00" in normalized:
        raise ValueError("report contains NUL")
    lines = normalized.split("\n")
    while lines and lines[-1] == "":
        lines.pop()
    if len(lines) < 5:
        raise ValueError("report has fewer than five non-terminator lines")
    if lines[0] != "AI-DAILY v1":
        raise ValueError("report does not begin with AI-DAILY v1")
    if lines[1] != f"date={expected_date}":
        raise ValueError("report date does not match the requested day")
    if lines[2] != "source=agent":
        raise ValueError("report source is not agent")
    if lines[3] != "---":
        raise ValueError("report lacks the header separator")
    body = "\n".join(lines[4:]).strip()
    if not body:
        raise ValueError("report body is empty")
    for section in EXPECTED_SECTIONS:
        if section not in lines[4:]:
            raise ValueError(f"report lacks required section: {section}")
    return ReportBody(normalized, expected_date, body)


def read_board_report(lines: Sequence[str], wanted_path: str) -> str | None:
    """Extract the last report body printed by the script's exact cat command."""
    command = f"NSH> cat {wanted_path}"
    starts = [index for index, line in enumerate(lines) if line == command]
    for command_index in reversed(starts):
        body_start = next(
            (
                index
                for index in range(command_index + 1, len(lines))
                if lines[index] == "AI-DAILY v1"
            ),
            None,
        )
        if body_start is None:
            continue
        body = []
        for line in lines[body_start:]:
            if (
                line.startswith("nsh>")
                or line.startswith("NSH>")
                or any(re.fullmatch(pattern, line) for pattern in MARKERS.values())
            ):
                break
            body.append(line)
        return "\n".join(body).rstrip("\n")
    return None


def classify(
    text: str,
    expected_date: str,
    expected_content: str | None = None,
    expected_trace_id: str | None = None,
) -> Result:
    """Require one ordered, attributable success path."""
    lines = split_lines(text)
    events = list(_events(lines))

    def fail(reason: str) -> Result:
        return Result(False, reason, "")

    # The caller must prove it held a valid backup and removed the live file
    # before waiting for the automatic request.
    held_events = [e for e in events if e.kind in {"HOLD", "REMOVED"}]
    if not held_events or held_events[0].kind != "HOLD":
        return fail("missing HOLD OK before removal")
    hold_name = held_events[0].match.group(1)
    if not REPORT_DIR_RE.fullmatch(f"/data/velaguard/reports/{hold_name}"):
        return fail("HOLD OK names an unexpected backup file")
    if not any(
        e.kind == "REMOVED"
        and REPORT_PATH_RE.fullmatch(e.match.group(1))
        and REPORT_PATH_RE.fullmatch(e.match.group(1)).group(1) == expected_date
        and e.index > held_events[0].index
        for e in held_events
    ):
        # Keep the reason generic when a test intentionally rewrites every
        # occurrence of the date: reviewers then see the failed path check,
        # rather than an assertion that today's date was never present.
        return fail(
            "missing REMOVED OK for expected path after HOLD; "
            "check the report date/path"
        )

    requests = [e for e in events if e.kind == "REQUEST"]
    if not requests:
        return fail("board did not print vghmi: daily report requested")
    # The capture can include an unrelated earlier request.  Select the last
    # complete request on the stream and require all later evidence for it.
    for request in reversed(requests):
        submitted = next(
            (
                e
                for e in events
                if e.kind == "SUBMITTED" and e.index > request.index
            ),
            None,
        )
        if submitted is None:
            continue
        begins = [
            e
            for e in events
            if e.kind == "TRACE_BEGIN" and e.index > submitted.index
        ]
        if not begins:
            continue
        begin = begins[0]
        trace_id = begin.match.group(1)
        if expected_trace_id and trace_id != expected_trace_id:
            continue
        ends = [
            e
            for e in events
            if e.kind == "TRACE_END"
            and e.index > begin.index
            and e.match.group(1) == trace_id
        ]
        if not ends:
            continue
        end = ends[0]
        if end.match.group(2) != "ok":
            continue
        if int(end.match.group(4)) <= 0:
            continue

        writes = [
            e
            for e in events
            if e.kind == "WRITE_INTENT"
            and begin.index < e.index < end.index
        ]
        execs = [
            e
            for e in events
            if e.kind == "WRITE_EXEC"
            and begin.index < e.index < end.index
        ]
        if not writes or not execs:
            continue
        write = writes[-1]
        path, content = parse_write_intent(write.match.group(1))
        wanted_path = f"/data/velaguard/reports/daily-{expected_date}.md"
        match = REPORT_PATH_RE.fullmatch(path or "")
        if path != wanted_path or match is None or match.group(1) != expected_date:
            continue
        if content is None:
            continue
        try:
            report = validate_report(content, expected_date)
        except ValueError:
            continue
        if expected_content is not None and normalize_report(expected_content) != report.text:
            continue
        board_text = read_board_report(lines, wanted_path)
        if board_text is None:
            continue
        try:
            board_report = validate_report(board_text, expected_date)
        except ValueError:
            continue
        if board_report.text.rstrip("\n") != report.text.rstrip("\n"):
            continue

        verified = next(
            (
                e
                for e in events
                if e.kind == "VERIFIED"
                and e.index > end.index
                and e.match.group(1) == wanted_path
            ),
            None,
        )
        if verified is None:
            continue
        reported_bytes = verified.match.group(2)
        if reported_bytes is not None and int(reported_bytes) != len(report.text.encode("utf-8")):
            continue

        restored = next(
            (
                e
                for e in events
                if e.kind == "RESTORED"
                and e.index > verified.index
                and e.match.group(1) == wanted_path
                and e.match.group(2) == hold_name
            ),
            None,
        )
        if restored is None:
            continue

        # Cleanup must be last so a later restore cannot be hidden by an
        # apparently successful earlier copy.
        later_hold_state = [
            e
            for e in events
            if e.kind in {"RESTORED", "CLEANUP"} and e.index > restored.index
        ]
        if any(
            e.kind == "RESTORED" and e.match.group(1) != wanted_path
            for e in later_hold_state
        ):
            continue
        cleanup = next(
            (
                e
                for e in events
                if e.kind == "CLEANUP"
                and e.index > restored.index
                and e.match.group(1) == hold_name
            ),
            None,
        )
        if cleanup is None:
            continue

        evidence = "\n".join(
            lines[e.index]
            for e in (held_events[0], *[
                x for x in held_events
                if x.kind == "REMOVED" and x.index > held_events[0].index
            ][:1], request, submitted, begin, write, execs[0], end,
            verified, restored, cleanup)
        )
        evidence += f"\nBOARD_READ validated bytes={len(board_text.encode('utf-8'))}"
        return Result(True, "ordered board-driven daily report evidence is complete", evidence)

    return fail(
        "no complete request/submit/valid trace/exact write/success/file/restore sequence"
    )


def normalize_report(text: str) -> str:
    return text.replace("\r\n", "\n").replace("\r", "\n").rstrip("\n")


def _read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="strict")


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path, help="full COM console capture")
    parser.add_argument(
        "--date",
        default=dt.datetime.now().astimezone().date().isoformat(),
        help="report date in YYYY-MM-DD (default: local date)",
    )
    parser.add_argument(
        "--report",
        type=Path,
        help="optional expected report body, for example a held backup",
    )
    parser.add_argument("--trace-id", help="optional exact 16-hex trace id")
    args = parser.parse_args(argv)

    try:
        text = _read_text(args.capture)
        expected_content = _read_text(args.report) if args.report else None
        result = classify(text, args.date, expected_content, args.trace_id)
    except (OSError, UnicodeError, ValueError) as error:
        print(f"FAIL {error}", file=sys.stderr)
        return 1

    print(("PASS " if result.ok else "FAIL ") + result.reason)
    if result.evidence:
        print("EVIDENCE:")
        print(result.evidence)
    return 0 if result.ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
