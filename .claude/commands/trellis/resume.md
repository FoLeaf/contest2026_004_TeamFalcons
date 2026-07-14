---
description: Select an existing Trellis task for this Claude session without starting it
argument-hint: <task-name-or-path>
---

# Resume Existing Task

Attach this Claude session to an existing live Trellis task without changing
the task status or running lifecycle hooks.

Task argument: `$ARGUMENTS`

If `$ARGUMENTS` is empty, stop and report:

```text
Usage: /trellis:resume <task-name-or-path>
```

Do not guess a task and do not call `task.py start`.

## Step 1: Select the task for this session

Treat `$ARGUMENTS` as one opaque task name or path and run:

```bash
python3 ./.trellis/scripts/task.py select "$ARGUMENTS"
```

If selection fails, stop and report the CLI error. Do not continue with a
different task.

## Step 2: Confirm the session-scoped pointer

```bash
python3 ./.trellis/scripts/task.py current --source
```

The selected path must match `$ARGUMENTS` after task-name resolution, and the
source must be session-scoped.

## Step 3: Load context and route by unchanged status

```bash
python3 ./.trellis/scripts/get_context.py
python3 ./.trellis/scripts/get_context.py --mode phase
```

Use the selected task's existing `task.json.status` and artifact presence to
choose the next phase exactly as `/trellis:continue` does:

- `planning` → stay in Phase 1 and load the required planning step. Only run
  `task.py start` later, after artifact review and explicit implementation
  approval.
- `in_progress` → load Phase 2 step 2.1 for Claude Code.
- `completed` or stale → stop and report that the task cannot be resumed as
  active work.

Summarize the selected task, session-scoped source, unchanged status, and next
workflow step.
