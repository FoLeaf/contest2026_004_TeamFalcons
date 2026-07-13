# Technical Design: OpenCode planning-task resume

## 1. Decision

Add a project-local, status-preserving task-selection operation and expose it
to OpenCode as `/trellis:resume <task>`.

The lifecycle vocabulary is deliberately split into three operations:

| Operation | Purpose | Pointer | Task status | Hooks |
| --- | --- | --- | --- | --- |
| `/trellis:start` | Initialize an OpenCode session and load context | Read only | Unchanged | None |
| `task.py select` / `/trellis:resume` | Attach this session to an existing task | Set | Unchanged | None |
| `task.py start` | Activate reviewed work for implementation | Set | `planning` → `in_progress` | `after_start` |

This prevents a new AI session from having to choose between remaining in
`no_task` and incorrectly advancing a planning task into implementation.

## 2. Alternatives considered

### A. Reuse `task.py start`

Rejected. `cmd_start()` intentionally combines pointer assignment, status
transition, and lifecycle-hook execution. Making it conditional for resume
would overload an established command and make approval boundaries ambiguous.

### B. Write `.trellis/.runtime/sessions/*.json` from the OpenCode command

Rejected. This bypasses task validation, canonicalization, session-key
resolution, metadata handling, and future runtime changes. It would also make
the command depend on a private storage format.

### C. Add `task.py select` and `/trellis:resume`

Selected. It adds one narrow public lifecycle primitive, reuses
`set_active_task()`, and leaves `start` semantics intact.

## 3. Components and boundaries

### 3.1 `task.py select <task>`

`cmd_select()` owns CLI-level validation and user-facing errors:

1. Require a non-empty task reference.
2. Resolve names and paths using `resolve_task_dir()`.
3. Accept only a live, top-level task directory under `.trellis/tasks/` with a
   `task.json`; explicitly reject `archive/**` and paths outside the active-task
   root.
4. Require a stable context key through `resolve_context_key()`.
5. Call `set_active_task()` with the canonical repository-relative path.
6. Print the selected path and its `session:<context-key>` source.

It must not import or call JSON writers or lifecycle hooks. Consequently,
selection cannot change task fields or trigger `after_start`.

The existing session-bridge command allow-list in
`common/active_task.py` gains `select`. Any command-argument matching that is
required by the bridge treats `select` like `start`, because both carry a task
reference. The pointer remains session-scoped; no global fallback is added.

### 3.2 OpenCode command

`.opencode/commands/trellis/resume.md` is a prompt command, not another state
store. OpenCode 1.17.18 and its official command documentation support `$1` for
the first positional argument.

The command flow is:

```text
/trellis:resume <task>
  → validate that $1 was supplied
  → task.py select "$1"
  → task.py current --source
  → get_context.py
  → get_context.py --mode phase
  → route by unchanged task status and artifact presence
```

If selection fails, the command stops and reports the CLI error. It must never
fall through to `task.py start`.

### 3.3 Workflow documentation

The shared workflow documents the new selection primitive because lifecycle
meaning is shared runtime knowledge. Only OpenCode receives a new slash command
because this repair is intentionally project-local and platform-scoped.

- `/trellis:start` says it initializes context and points users to
  `/trellis:resume` when an existing task needs attachment.
- `/trellis:continue` says it operates on the task already selected in this
  session. With `no_task`, it routes existing-task users to resume and new-work
  users to normal task triage.
- `.trellis/workflow.md` adds `select` to the CLI reference and current-task
  contract, while preserving Phase 1.4 as the only route to implementation
  activation.

## 4. Data and state flow

The task directory and `task.json` are read-only inputs. The only write is the
current session record managed by `set_active_task()`:

```text
task reference
  → resolve active task directory
  → canonical repository-relative task path
  → resolve current session context key
  → update .trellis/.runtime/sessions/<context-key>.json
  → next OpenCode turn reads the pointer
  → workflow breadcrumb uses the unchanged task.json status
```

Different OpenCode windows resolve different context keys, so selecting a task
in one window cannot retarget another window.

## 5. Error handling

- Missing argument: exit non-zero with command usage.
- Missing or invalid task: exit non-zero; do not alter the existing pointer.
- Archived or non-active task path: exit non-zero with an active-task hint.
- Missing `task.json`: treat the directory as invalid.
- Missing session identity: exit non-zero and explain that selection must run
  inside an AI session exposing `TRELLIS_CONTEXT_ID` or a recognized native
  session ID.
- Pointer write failure: exit non-zero without reporting success.

No degraded success mode is allowed for `select`, because a status-preserving
resume that does not actually persist the pointer would reproduce the original
failure.

## 6. Compatibility, rollout, and rollback

This is an additive CLI command and one additive OpenCode command file.
Existing `create`, `start`, `current`, and `finish` call paths remain unchanged.
The repair does not alter OpenCode provider/model/plugin configuration or the
global Trellis package.

Rollback is file-local: remove `resume.md`, remove the `select` parser/handler
and session-bridge allow-list entry, and restore the three documentation edits.
No task-data migration is required because `task.json` format is unchanged.

## 7. Verification strategy

Verification covers each boundary independently:

1. Python syntax and CLI help expose `select`.
2. A disposable context ID selects a live planning task, and
   `current --source` reports the same session source.
3. SHA-256 of the target `task.json` is identical before and after selection.
4. Missing identity, invalid task, archived task, and missing `task.json` fail
   without replacing an existing pointer.
5. A hook spy confirms `cmd_select()` never calls `run_task_hooks()`.
6. Searches confirm all three lifecycle terms are consistent across the
   workflow and OpenCode commands.
7. A fresh OpenCode session runs `/trellis:resume <planning-task>` and receives
   the planning breadcrumb on the next turn without a status transition.
