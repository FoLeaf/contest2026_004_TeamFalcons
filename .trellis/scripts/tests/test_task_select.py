#!/usr/bin/env python3
"""Regression tests for status-preserving task selection."""

from __future__ import annotations

import argparse
import sys
import tempfile
import unittest
from contextlib import redirect_stdout
from io import StringIO
from pathlib import Path
from unittest.mock import patch

SCRIPTS_DIR = Path(__file__).resolve().parents[1]
if str(SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPTS_DIR))

import task  # noqa: E402
from common import active_task  # noqa: E402
from common.active_task import ActiveTask  # noqa: E402


class TaskSelectTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temp_dir = tempfile.TemporaryDirectory()
        self.repo_root = Path(self.temp_dir.name)
        self.tasks_dir = self.repo_root / ".trellis" / "tasks"
        self.live_dir = self.tasks_dir / "07-13-live-task"
        self.live_dir.mkdir(parents=True)
        self.task_json = self.live_dir / "task.json"
        self.task_json.write_bytes(b'{"status":"planning","marker":"unchanged"}\n')

    def tearDown(self) -> None:
        self.temp_dir.cleanup()

    def _select(self, task_ref: str, context_key: str | None) -> tuple[int, object]:
        selected = ActiveTask(
            ".trellis/tasks/07-13-live-task",
            "session",
            context_key,
        )
        with (
            patch.object(task, "get_repo_root", return_value=self.repo_root),
            patch.object(task, "resolve_context_key", return_value=context_key),
            patch.object(task, "set_active_task", return_value=selected) as setter,
            patch.object(task, "read_json", side_effect=AssertionError("task JSON read")),
            patch.object(task, "write_json", side_effect=AssertionError("task JSON write")),
            patch.object(task, "run_task_hooks", side_effect=AssertionError("hook called")),
            redirect_stdout(StringIO()),
        ):
            result = task.cmd_select(argparse.Namespace(dir=task_ref))
        return result, setter

    def test_select_preserves_task_json_and_sets_session_pointer(self) -> None:
        before = self.task_json.read_bytes()

        result, setter = self._select("live-task", "opencode_session_test")

        self.assertEqual(result, 0)
        self.assertEqual(self.task_json.read_bytes(), before)
        setter.assert_called_once_with(
            ".trellis/tasks/07-13-live-task",
            self.repo_root,
        )

    def test_select_requires_session_identity(self) -> None:
        result, setter = self._select("live-task", None)

        self.assertEqual(result, 1)
        setter.assert_not_called()

    def test_select_rejects_archived_and_malformed_tasks(self) -> None:
        archived = self.tasks_dir / "archive" / "2026-07" / "07-13-old-task"
        archived.mkdir(parents=True)
        (archived / "task.json").write_text('{"status":"completed"}\n', encoding="utf-8")
        malformed = self.tasks_dir / "07-13-missing-json"
        malformed.mkdir()

        for task_ref in (str(archived), str(malformed)):
            with self.subTest(task_ref=task_ref):
                result, setter = self._select(task_ref, "opencode_session_test")
                self.assertEqual(result, 1)
                setter.assert_not_called()

    def test_cursor_bridge_matches_select_task_reference(self) -> None:
        ticket = {
            "subcommands": [
                {
                    "name": "select",
                    "task_ref": ".trellis/tasks/07-13-live-task",
                }
            ]
        }

        with patch.object(
            sys,
            "argv",
            ["task.py", "select", ".trellis/tasks/07-13-live-task"],
        ):
            self.assertTrue(active_task._pending_ticket_matches_args(ticket, self.repo_root))

        with patch.object(sys, "argv", ["task.py", "select", "different-task"]):
            self.assertFalse(active_task._pending_ticket_matches_args(ticket, self.repo_root))


if __name__ == "__main__":
    unittest.main()
