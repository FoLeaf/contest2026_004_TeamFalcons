# Fix OpenCode planning task resume

## Goal

Allow a new OpenCode session to attach to an existing Trellis task while it is
still in planning, without falsely activating implementation or changing task
lifecycle state.

## Background

- OpenCode session `ses_0a4080afbffe5942aQF53oHSbe` repeatedly received the
  `no_task` workflow breadcrumb after `/trellis:start`.
- `/trellis:start` initializes session context; it does not attach an existing
  task to the current session.
- OpenCode propagates a stable session identity to shell commands through
  `TRELLIS_CONTEXT_ID`, so the session-scoped task pointer can be written safely.
- `task.py start <task>` is not planning-safe: it changes `planning` to
  `in_progress` and runs lifecycle hooks in addition to setting the pointer.
- The affected OpenCode session did not modify business code. It only appended
  user-confirmed RS485 decisions to that task's planning artifact.

## Requirements

### R1 — Planning-safe task selection

- Provide a public `task.py select <task>` command that resolves and validates
  an existing active task directory using the same task-reference rules as
  other task lifecycle commands.
- The command must update only the current AI session's active-task pointer.
- The command must preserve every field in `task.json`, including `status`.
- The command must not run `after_start` or any other lifecycle hook.
- The command must fail with a clear non-zero error when no stable session
  identity is available; it must not fall back to a shared/global pointer.

### R2 — OpenCode resume entry point

- Provide `/trellis:resume <task>` under the project-local OpenCode command
  directory.
- The command must use OpenCode's documented argument placeholder syntax and
  invoke the planning-safe selection operation.
- After selection it must report the selected task source and route the AI to
  the correct planning or execution phase based on the unchanged task status.

### R3 — Unambiguous workflow guidance

- `/trellis:start` must describe itself as session initialization, not task
  attachment or implementation activation.
- `/trellis:continue` must explain that it continues the task already selected
  in the current session and direct users with `no_task` plus an existing task
  to `/trellis:resume <task>`.
- `.trellis/workflow.md` must distinguish:
  1. session initialization (`/trellis:start`),
  2. status-preserving selection (`task.py select` / `/trellis:resume`), and
  3. implementation activation (`task.py start` after planning review).

### R4 — Compatibility and containment

- Keep the repair project-local and OpenCode-focused; do not modify the global
  npm installation, `node_modules`, or unrelated platform integrations.
- Preserve existing `task.py start`, `create`, `current`, and `finish`
  semantics.
- Preserve session isolation between simultaneous AI windows.

## Acceptance Criteria

- [x] In a fresh OpenCode session with a stable context ID and no active task,
      selecting an existing `planning` task changes `task.py current --source`
      from no task to that task's session-scoped path.
- [x] The selected task's `task.json` is byte-for-byte unchanged by selection,
      and its status remains `planning`.
- [x] No lifecycle hook runs during selection.
- [x] Selection without a stable context ID exits non-zero and explains that a
      session identity is required.
- [x] Selecting an unknown, archived, or otherwise invalid active-task
      reference exits non-zero without changing the current pointer.
- [x] `/trellis:resume <task>` expands the supplied OpenCode command argument,
      performs selection, confirms the source, and loads the next phase without
      calling `task.py start`.
- [x] `/trellis:start`, `/trellis:continue`, and `.trellis/workflow.md` use the
      three distinct lifecycle terms consistently.
- [x] Existing task CLI behavior and relevant tests/checks still pass.

## Out of Scope

- Upstreaming the command to the published Trellis CLI or synchronizing all AI
  platform command directories.
- Automatically guessing which active task a new session intended to resume.
- Changing OpenCode provider, model, plugin, or credential configuration.
- Reorganizing the RS485 task artifacts; that cleanup remains follow-up work in
  `.trellis/tasks/07-13-resolve-usart3-rs485-vcp-conflict`.
