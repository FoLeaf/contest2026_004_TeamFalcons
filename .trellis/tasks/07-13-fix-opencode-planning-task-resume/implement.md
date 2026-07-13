# Implementation Plan: OpenCode planning-task resume

## Preconditions and review gate

- [x] `prd.md` and `design.md` have been reviewed by the user.
- [x] User explicitly approves entering implementation.
- [x] Run `python3 ./.trellis/scripts/task.py start
      .trellis/tasks/07-13-fix-opencode-planning-task-resume` only after that
      approval.
- [x] Load `trellis-before-dev` before editing runtime or command files.

## Step 1 — Add the status-preserving CLI operation

- [x] Add `select` to the usage text, examples, argument parser, and command
      dispatcher in `.trellis/scripts/task.py`.
- [x] Implement `cmd_select()` as a narrow handler that:
  - resolves the supplied task reference;
  - verifies it is a live top-level directory under `.trellis/tasks/`;
  - requires `task.json`;
  - requires a stable session identity;
  - calls `set_active_task()` exactly once;
  - reports the selected path and session source;
  - performs no task JSON writes and runs no hooks.
- [x] Add `select` to `TASK_SESSION_COMMANDS` in
      `.trellis/scripts/common/active_task.py` and make bridge argument matching
      treat `select` and `start` consistently.
- [x] Keep `cmd_start()` behavior unchanged.

Rollback point: revert only the `task.py` and `active_task.py` changes. No task
data migration or cleanup should be necessary beyond deleting disposable test
session records.

## Step 2 — Add the OpenCode resume command

- [x] Create `.opencode/commands/trellis/resume.md`.
- [x] Use OpenCode's documented `$1` positional placeholder for the task
      reference.
- [x] Stop with a clear usage message if `$1` is absent.
- [x] Run `task.py select`, then `task.py current --source`, then load session
      and phase context.
- [x] Route based on the unchanged task status and artifact presence.
- [x] State explicitly that the command must not call `task.py start`.

Rollback point: remove `resume.md`; the Python command remains independently
usable and existing OpenCode commands remain valid.

## Step 3 — Synchronize lifecycle guidance

- [x] Update `.opencode/commands/trellis/start.md` to distinguish session
      initialization from task selection and activation.
- [x] Update `.opencode/commands/trellis/continue.md` to require a current
      selection and route `no_task` + existing-task users to
      `/trellis:resume <task>`.
- [x] Update `.trellis/workflow.md` CLI reference and current-task mechanism to
      define `select` as status-preserving and hook-free.
- [x] Update the `task.py create --no-start` guidance so planning attachment
      points to `task.py select`, not implementation activation.
- [x] Preserve Phase 1.4 wording: only reviewed planning enters implementation
      via `task.py start`.
- [x] Search all touched OpenCode and workflow files for contradictory uses of
      “start”, “resume”, “select”, and “continue”.

## Step 4 — Automated and isolated verification

- [x] Compile the changed Python modules:

  ```bash
  python3 -m py_compile \
    .trellis/scripts/task.py \
    .trellis/scripts/common/active_task.py
  ```

- [x] Confirm parser/help registration:

  ```bash
  python3 ./.trellis/scripts/task.py --help
  python3 ./.trellis/scripts/task.py select --help
  ```

- [x] With a disposable `TRELLIS_CONTEXT_ID`, hash the target planning
      `task.json`, select it, query `current --source`, hash it again, and
      require equal hashes and unchanged `planning` status.
- [x] Under a second disposable context ID, establish a known pointer; attempt
      invalid, archived, and missing-`task.json` selections; verify each exits
      non-zero and the known pointer remains selected.
- [x] Run selection in an environment with all recognized Trellis/platform
      session identifiers removed; verify non-zero exit and the session-
      identity hint.
- [x] Invoke `cmd_select()` with `run_task_hooks` replaced by a failing spy;
      verify selection succeeds without touching the spy.
- [x] Clear every disposable session pointer with `task.py finish` under its
      matching context ID.
- [x] Review whitespace checks and a focused view of only the planned files.

## Step 5 — Fresh OpenCode acceptance

- [x] Record the chosen planning task's status and file hash.
- [x] Start a fresh OpenCode session in the repository.
- [x] Run `/trellis:start`, confirm it initializes context but does not select a
      task.
- [x] Run `/trellis:resume <planning-task>`.
- [x] Confirm `task.py current --source` reports the fresh OpenCode session and
      the next turn receives `Task: <id> (planning)` rather than `no_task`.
- [x] Confirm the task status and hash are unchanged.

If host-level OpenCode interaction cannot be automated safely, stop at this
single manual acceptance step and give the user the exact command and expected
output; all lower-level state and CLI behavior must already be verified.

## Step 6 — Quality and finish workflow

- [x] Load and run `trellis-check` against every PRD acceptance criterion.
- [x] Decide whether the new lifecycle contract belongs in `.trellis/spec/`;
      update the owning spec if it is durable project knowledge.
- [x] Do not mix cleanup of the RS485 task artifacts into this change.
- [ ] Present the focused diff and verification evidence before committing.
- [ ] Follow Trellis Phase 3.4 commit and finish steps only with the user's
      requested repository workflow.
