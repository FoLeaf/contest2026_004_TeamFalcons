#!/usr/bin/env python3
"""Offline tests for the daily-report acceptance log classifier."""

from __future__ import annotations

import importlib.util
import json
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


def report_with_detailed_observation() -> str:
    lines = REPORT.splitlines()
    body = [
        "通信质量",
        "总线成功 100%（28654/28654），超时 0 次，CRC 错误 0。",
        "近窗成功 100%（483/483），超时 0 次，CRC 错误 0。",
        "最差从站 1：成功 100%（28654/28654）。",
    ]
    body.extend(
        f"点位 {index:02d}：在线，读值正常。"
        for index in range(1, 15)
    )
    body.extend(
        [
            "点位在线",
            "14 个点位全部在线，最近一轮采集无超时和 CRC 错误。",
            "温度点位当前读数为 24.0°C，湿度与环境数据在规则范围内。",
            "告警规则已完成本轮确定性评估，没有新建或持续告警。",
            "异常时间线",
            "本次上电未记录离线、告警升级或停止更新。",
            "结论：本轮自动日报由板端 Agent 按当天采集数据生成。",
        ]
    )
    detailed = "\n".join(lines[:4] + body)
    assert 700 < len(detailed.encode("utf-8")) <= 1400
    return detailed


def write_line_for(report: str, *, truncated: bool = False) -> str:
    payload = json.dumps(
        {
            "path": "/data/velaguard/reports/daily-2026-10-19.md",
            "content": report,
        },
        ensure_ascii=False,
        separators=(",", ":"),
    )
    if truncated:
        # The board logger's "%.500s" cap applies to encoded bytes.  Keep
        # the replacement off the wire: the fake console carries text.
        payload = payload.encode("utf-8")[:500].decode("utf-8", "ignore")
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
    report: str = REPORT,
    truncate_intent: bool = False,
    restore_hold: str = "daily-2026-10-19.hold.md",
    cleanup_hold: str = "daily-2026-10-19.hold.md",
) -> str:
    t = "0123456789abcdef"
    lines = [
        "HOLD OK hold=daily-2026-10-19.hold.md",
        "REMOVED OK path=/data/velaguard/reports/daily-2026-10-19.md",
    ]
    if request:
        lines.append("vghmi: daily report requested")
    if submit:
        lines.append("[vgagent] round submitted: 124 bytes")
    if begin:
        lines.append(f"[trace:{t}] BEGIN chat=velaguard:13 chan=local_client")
    if intent:
        lines.append(write_line_for(report, truncated=truncate_intent).rstrip("\n"))
    if execute:
        lines.append("[tools] Executing tool: write_file")
    lines.append(
        f"[trace:{t}] END status={end_status} iters=4 tools={tools} "
        "llm_ms=2000 elapsed=3s backend=0"
    )
    if verify:
        lines.append(
            "VERIFY OK path=/data/velaguard/reports/daily-2026-10-19.md "
            f"bytes={len(report.encode('utf-8'))}"
        )
    if verify:
        lines.append("DAILY_ACCEPT> cat /data/velaguard/reports/daily-2026-10-19.md")
        lines.extend(report.splitlines())
        lines.append("nsh>")
    if restore:
        lines.append(
            "RESTORE OK path=/data/velaguard/reports/daily-2026-10-19.md "
            f"hold={restore_hold} cmp=identical"
        )
    if cleanup:
        lines.append(f"CLEANUP OK hold={cleanup_hold}")
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

    def test_long_report_with_truncated_write_intent_passes(self) -> None:
        long_report = report_with_detailed_observation()
        text = trace(report=long_report, truncate_intent=True)
        intent = next(line for line in text.splitlines() if "[agent] Tool call" in line)
        self.assertTrue(intent.split("args=", 1)[1].startswith('{"path":"'))
        self.assertFalse(intent.split("args=", 1)[1].endswith("}"))
        result = CLASSIFIER.classify(text, DATE, long_report)
        self.assertTrue(result.ok, result.reason)

    def test_truncated_intent_wrong_path_fails(self) -> None:
        text = trace(
            report=report_with_detailed_observation(),
            truncate_intent=True,
        ).replace(
            '{"path":"/data/velaguard/reports/daily-2026-10-19.md"',
            '{"path":"/data/velaguard/reports/daily-2026-10-18.md"',
        )
        result = CLASSIFIER.classify(text, DATE, REPORT)
        self.assertFalse(result.ok)
        self.assertIn("path", result.reason)

    def test_complete_intent_requires_matching_cat_body(self) -> None:
        altered = REPORT.replace("暂无异常。", "生成过程中发生错误。")
        result = CLASSIFIER.classify(trace(report=altered), DATE, REPORT)
        self.assertFalse(result.ok)

    def test_complete_request_before_hold_does_not_count(self) -> None:
        early = "vghmi: daily report requested\n"
        result = CLASSIFIER.classify(early + trace(request=False), DATE, REPORT)
        self.assertFalse(result.ok)

    def test_duplicate_submit_after_request_without_new_trace_fails(self) -> None:
        text = trace(request=False) + "\n" + (
            "vghmi: daily report requested\n"
            "[vgagent] round submitted: 124 bytes\n"
        )
        result = CLASSIFIER.classify(text, DATE, REPORT)
        self.assertFalse(result.ok)

    def test_cleanup_with_wrong_hold_fails(self) -> None:
        result = CLASSIFIER.classify(
            trace(cleanup_hold="daily-2026-10-19.baseline.md"),
            DATE,
            REPORT,
        )
        self.assertFalse(result.ok)

    def test_restore_failure_fails(self) -> None:
        result = CLASSIFIER.classify(
            trace(restore_hold="daily-2026-10-19.baseline.md"),
            DATE,
            REPORT,
        )
        self.assertFalse(result.ok)


if __name__ == "__main__":
    unittest.main()
