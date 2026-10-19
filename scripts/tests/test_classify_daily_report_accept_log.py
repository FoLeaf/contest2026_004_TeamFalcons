#!/usr/bin/env python3
"""Offline tests for the daily-report acceptance log classifier."""

from __future__ import annotations

import importlib.util
import sys
import unittest
from pathlib import Path


PATH = Path(__file__).resolve().parents[1] / "classify_daily_report_accept_log.py"
SPEC = importlib.util.spec_from_file_location("daily_classifier", PATH)
assert SPEC is not None and SPEC.loader is not None
CLASSIFIER = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = CLASSIFIER
SPEC.loader.exec_module(CLASSIFIER)

DATE = "2026-10-19"
REPORT = (
    "AI-DAILY v1\n"
    f"date={DATE}\n"
    "source=agent\n"
    "---\n"
    "通信质量\n"
    "总线成功 100%。\n"
    "点位在线\n"
    "全部点位在线。\n"
    "异常时间线\n"
    "暂无异常。"
)


def write_line(intent: bool = True, path_suffix: str = "") -> str:
    if not intent:
        return ""
    payload = (
        '{"path":"/data/velaguard/reports/daily-2026-10-19'
        + path_suffix
        + '.md","content":"AI-DAILY v1\\ndate=2026-10-19\\nsource=agent\\n---\\n'
        + '通信质量\\n总线成功 100%。\\n点位在线\\n全部点位在线。\\n异常时间线\\n暂无异常。"}'
    )
    return f"[agent] Tool call: write_file args={payload}\n"


def trace(
    *,
    request: bool = True,
    submit: bool = True,
    begin: bool = True,
    intent: bool = True,
    execute: bool = True,
    end_status: str = "ok",
    tools: int = 3,
    verify: bool = True,
    restore: bool = True,
    cleanup: bool = True,
) -> str:
    t = "0123456789abcdef"
    lines = [
        "HOLD OK hold=daily-2026-10-19_acceptance.hold.md",
        "REMOVED OK path=/data/velaguard/reports/daily-2026-10-19.md",
    ]
    if request:
        lines.append("vghmi: daily report requested")
    if submit:
        lines.append("[vgagent] round submitted: 124 bytes")
    if begin:
        lines.append(f"[trace:{t}] BEGIN chat=velaguard:13 chan=local_client")
    if intent:
        lines.append(write_line().rstrip("\n"))
    if execute:
        lines.append("[tools] Executing tool: write_file")
    lines.append(
        f"[trace:{t}] END status={end_status} iters=4 tools={tools} "
        "llm_ms=2000 elapsed=3s backend=0"
    )
    if verify:
        lines.append(
            "VERIFY OK path=/data/velaguard/reports/daily-2026-10-19.md "
            f"bytes={len(REPORT.encode('utf-8'))}"
        )
    if verify:
        lines.append("NSH> cat /data/velaguard/reports/daily-2026-10-19.md")
        lines.extend(REPORT.splitlines())
        lines.append("nsh>")
    if restore:
        lines.append(
            "RESTORE OK path=/data/velaguard/reports/daily-2026-10-19.md "
            "hold=daily-2026-10-19_acceptance.hold.md cmp=identical"
        )
    if cleanup:
        lines.append("CLEANUP OK hold=daily-2026-10-19_acceptance.hold.md")
    return "\n".join(lines) + "\n"


class ClassifierTests(unittest.TestCase):
    def test_complete_ordered_evidence_passes(self) -> None:
        result = CLASSIFIER.classify(trace(), DATE, REPORT)
        self.assertTrue(result.ok, result.reason)
        self.assertIn("[trace:0123456789abcdef]", result.evidence)

    def test_wrong_path_fails(self) -> None:
        text = trace().replace(
            "/data/velaguard/reports/daily-2026-10-19.md",
            "/data/velaguard/reports/daily-2026-10-18.md",
        )
        result = CLASSIFIER.classify(text, DATE, REPORT)
        self.assertFalse(result.ok)
        self.assertIn("path", result.reason)

    def test_failed_trace_fails(self) -> None:
        result = CLASSIFIER.classify(trace(end_status="fail"), DATE, REPORT)
        self.assertFalse(result.ok)

    def test_zero_tools_fails(self) -> None:
        result = CLASSIFIER.classify(trace(tools=0), DATE, REPORT)
        self.assertFalse(result.ok)

    def test_out_of_order_restore_fails(self) -> None:
        text = trace()
        lines = text.splitlines()
        restore_index = next(i for i, line in enumerate(lines) if line.startswith("RESTORE"))
        cleanup_index = next(i for i, line in enumerate(lines) if line.startswith("CLEANUP"))
        lines[restore_index], lines[cleanup_index] = lines[cleanup_index], lines[restore_index]
        result = CLASSIFIER.classify("\n".join(lines) + "\n", DATE, REPORT)
        self.assertFalse(result.ok)

    def test_interleaved_older_request_does_not_steal_new_trace(self) -> None:
        old = "\n".join(
            [
                "vghmi: daily report requested",
                "[vgagent] round submitted: 124 bytes",
                "[trace:aaaaaaaaaaaaaaaa] BEGIN chat=old chan=local_client",
                "[trace:aaaaaaaaaaaaaaaa] END status=fail iters=1 tools=0",
            ]
        )
        result = CLASSIFIER.classify(old + "\n" + trace(), DATE, REPORT)
        self.assertTrue(result.ok)

    def test_missing_execution_fails(self) -> None:
        result = CLASSIFIER.classify(trace(execute=False), DATE, REPORT)
        self.assertFalse(result.ok)

    def test_invalid_report_body_fails(self) -> None:
        broken = REPORT.replace("source=agent", "source=firmware")
        text = trace().replace(
            'source=agent\\n---',
            'source=firmware\\n---',
        )
        result = CLASSIFIER.classify(text, DATE, broken)
        self.assertFalse(result.ok)

    def test_board_file_content_must_match_write_payload(self) -> None:
        text = trace().replace(
            "总线成功 100%。\n点位在线",
            "总线成功 90%。\n点位在线",
            1,
        )
        result = CLASSIFIER.classify(text, DATE, REPORT)
        self.assertFalse(result.ok)

    def test_prompt_framing_is_removed(self) -> None:
        text = trace().replace(
            "vghmi: daily report requested",
            "nsh> \x1b[Kvghmi: daily report requested",
        )
        result = CLASSIFIER.classify(text, DATE, REPORT)
        self.assertTrue(result.ok, result.reason)


if __name__ == "__main__":
    unittest.main()
