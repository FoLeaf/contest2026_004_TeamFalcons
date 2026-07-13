# Resume Existing Task

Attach this OpenCode session to an existing live Trellis task without changing
the task status or running lifecycle hooks.

Task argument: `$1`

If `$1` is empty, stop and report:

```text
Usage: /trellis:resume <task-name-or-path>
```

Do not guess a task and do not call `task.py start`.

## Step 1: Select the task for this session

Treat `$1` as one opaque task name or path and run:

```bash
python3 ./.trellis/scripts/task.py select "$1"
```

If selection fails, stop and report the CLI error. Do not continue with a
different task.

## Step 2: Confirm the session-scoped pointer

```bash
python3 ./.trellis/scripts/task.py current --source
```

The selected path must match `$1` after task-name resolution, and the source
must be session-scoped.

## Step 3: Load context and route by unchanged status

```bash
python3 ./.trellis/scripts/get_context.py
python3 ./.trellis/scripts/get_context.py --mode phase
```

Use the selected task's existing `task.json.status` and artifact presence to
choose the next phase exactly as `/trellis:continue` does:

- `planning` → stay in Phase 1; load the required planning step. Only run
  `task.py start` later, after artifact review and explicit implementation
  approval.
- `in_progress` → load Phase 2 step 2.1 for OpenCode.
- `completed` or stale → stop and report that the task cannot be resumed as
  active work.

Summarize the selected task, source, unchanged status, and next workflow step.
