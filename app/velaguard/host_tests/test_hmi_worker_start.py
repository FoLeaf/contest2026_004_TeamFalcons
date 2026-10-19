#!/usr/bin/env python3
"""Static wiring check for the HMI file worker.

The daily-report scheduler, agent-round pump and alarm-advice pump all run in
the file worker thread.  Starting that worker only from report-page callbacks
makes the first two depend on the user opening the page, which is the
2026-10-19 regression.  The backend must start it from acquisition startup.
"""

from __future__ import annotations

import pathlib
import re
import sys


ROOT = pathlib.Path(__file__).resolve().parents[1]
SOURCE = ROOT / "vg_ui_backend_board.c"


def function_body(text: str, name: str) -> str:
    match = re.search(
        rf"(?m)^(?:FAR\s+)?(?:static\s+)?(?:void|int|bool)\s+"
        rf"{re.escape(name)}\s*\([^)]*\)\s*\n?\{{",
        text,
    )
    if match is None:
        raise AssertionError(f"{name}() not found")
    start = match.end()
    depth = 1
    index = start
    while index < len(text) and depth:
        char = text[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
        index += 1
    if depth:
        raise AssertionError(f"{name}() is unbalanced")
    return text[start:index - 1]


def check(condition: bool, message: str) -> bool:
    print(f"[{'PASS' if condition else 'FAIL'}] {message}")
    return condition


def main() -> int:
    text = SOURCE.read_text(encoding="utf-8")
    worker = function_body(text, "ensure_file_worker_started")
    acq = function_body(text, "vg_ui_backend_acq_start")
    failures = 0

    failures += not check(
        acq.find("ensure_file_worker_started();")
        < acq.find("pthread_create("),
        "acquisition startup requests the file worker before creating acquisition",
    )
    failures += not check(
        "pthread_mutex_lock(&g_file_worker_lock);" in worker
        and "if(g_file_worker_started)" in worker
        and "g_file_worker_started = false;" in worker,
        "file-worker startup is single-instance and restores failure state",
    )
    failures += not check(
        "pthread_attr_setstacksize(&attr, VG_HMI_FILE_WORKER_STACKSIZE);"
        in worker,
        "file-worker stack is explicitly bounded",
    )
    failures += not check(
        "pthread_detach(tid);" in worker and "pthread_mutex_unlock(&g_file_worker_lock);" in worker,
        "success path detaches once and releases the startup lock",
    )

    if failures:
        print(f"test_hmi_worker_start: {failures} FAIL")
        return 1
    print("test_hmi_worker_start: ALL PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
