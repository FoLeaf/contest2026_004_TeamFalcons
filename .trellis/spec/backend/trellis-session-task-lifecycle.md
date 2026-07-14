# Trellis Session Task Lifecycle

## 1. Scope / Trigger

Use this contract whenever an AI session must create, select, resume, activate,
query, or clear a Trellis task. It is especially important when a new session
continues an existing task whose status is still `planning`.

The session pointer and the task lifecycle status are separate state. A command
must not change lifecycle state merely to make a task visible to one AI window.

## 2. Signatures

```text
python3 ./.trellis/scripts/task.py select <task>
python3 ./.trellis/scripts/task.py start <task>
python3 ./.trellis/scripts/task.py current --source
python3 ./.trellis/scripts/task.py finish
```

OpenCode user entry:

```text
/trellis:resume <task>
```

OpenCode 1.17.18 non-interactive acceptance uses the raw file-relative command
name, not the TUI presentation name:

```bash
opencode run --command trellis/resume <task>
```

Claude Code user entry:

```text
/trellis:resume <task>
```

Claude Code intentionally has no `/trellis:start`: its `SessionStart` hook
performs initialization automatically.

## 3. Contracts

### Selection

- Input `<task>`: a name, repository-relative path, or absolute path resolving
  to one live top-level directory under `.trellis/tasks/`.
- Required file: `<task>/task.json`.
- Required environment: a stable context key from `TRELLIS_CONTEXT_ID` or a
  recognized platform-native session identifier.
- Write: only `.trellis/.runtime/sessions/<context-key>.json` through
  `set_active_task()`.
- Read-only state: all bytes of `<task>/task.json`, including `status`.
- Forbidden side effects: task lifecycle hooks and shared/global pointers.
- Success output: canonical task path and `session:<context-key>` source.

### Activation

`task.py start` is the reviewed transition into implementation. It may set the
session pointer, changes `planning` to `in_progress`, and runs `after_start`.
Without session identity it retains its existing degraded behavior: lifecycle
transition may still occur even though the pointer is not persisted.

### OpenCode command roles

| Entry | Contract |
| --- | --- |
| `/trellis:start` | Initialize the OpenCode session and load context only |
| `/trellis:resume <task>` | Call `task.py select`, confirm source, then route by unchanged status |
| `/trellis:continue` | Continue the task already selected in this session |

### Claude Code session bridge and command roles

Claude Code passes `session_id`, `cwd`, and `transcript_path` to the project
`SessionStart` hook. The generated `.claude/hooks/session-start.py` must:

1. call `resolve_context_key(hook_input, platform="claude")`;
2. append `export TRELLIS_CONTEXT_ID=<resolved-key>` to the file named by
   `CLAUDE_ENV_FILE`;
3. inject the current Trellis session/workflow overview without changing any
   task state.

Later Claude Bash calls inherit the exported context key, so `task.py select`
and `task.py current --source` operate on the same conversation-specific
pointer.

| Entry | Contract |
| --- | --- |
| Claude `SessionStart` hook | Initialize context and bridge session identity; never activate a task |
| `/trellis:resume <task>` | Call `task.py select`, confirm session source, then route by unchanged status |
| `/trellis:continue` | Continue the task already selected in this Claude session |
| `/trellis:finish-work` | Enter finish flow only after quality and commit gates pass |

## 4. Validation & Error Matrix

| Condition | Required result |
| --- | --- |
| Missing task argument | Non-zero; show command usage |
| Unknown/non-directory task | Non-zero; pointer unchanged |
| Path outside live top-level `.trellis/tasks/` | Non-zero; pointer unchanged |
| Archived task | Non-zero; explain that archived tasks cannot be selected |
| Missing `task.json` | Non-zero; treat as invalid active task |
| Missing session identity | Non-zero; explain supported identity sources |
| Claude hook has no `CLAUDE_ENV_FILE` | Hook remains non-fatal, but no Bash bridge is written; later `select` must fail unless another recognized session env exists |
| Claude hook receives a valid session ID and env file | Append one shell-safe `TRELLIS_CONTEXT_ID` export for that conversation |
| Pointer write failure | Non-zero; never print success |
| Valid live task and context key | Zero; report canonical path and session source |

Validation must complete before `set_active_task()` so every error preserves
the caller's existing pointer.

## 5. Good / Base / Bad Cases

- **Good**: a fresh OpenCode window runs `/trellis:resume my-planning-task`;
  `current --source` reports that window's session key, `task.json` remains
  byte-identical, and the next turn injects `(planning)`.
- **Good**: a fresh Claude session receives `workflow-state=no_task`, then
  `/trellis:resume my-planning-task` reports a `session:claude_*` source while
  leaving `task.json` byte-identical.
- **Base**: `task.py create` already selected a new task in the same session;
  no resume operation is needed.
- **Bad**: using `task.py start` only to escape `no_task`; this bypasses the
  planning review gate and may run external lifecycle hooks.
- **Bad**: writing `.trellis/.runtime/sessions/*.json` from a platform command;
  this duplicates private canonicalization and metadata contracts.

## 6. Tests Required

Unit/regression assertions:

1. `select` calls `set_active_task()` once with the canonical live task path.
2. `task.json` bytes and status are unchanged.
3. JSON writers and lifecycle hooks are not called.
4. Missing identity, archived path, and missing `task.json` return non-zero and
   do not call the pointer writer.
5. Session-bridge matching checks the task reference for both `select` and
   `start`.
6. Two context IDs can select different tasks without contamination.

OpenCode end-to-end assertions:

1. A fresh session runs the resume command successfully.
2. The command reports `Source: session:opencode_<session-id>`.
3. The selected task remains `planning`.
4. The next turn injects `Task: <id> (planning)`, not `Status: no_task`.

Claude Code end-to-end assertions:

1. SessionStart output includes Trellis context and the current workflow-state.
2. Two hook inputs with distinct session IDs write distinct
   `TRELLIS_CONTEXT_ID` values to separate `CLAUDE_ENV_FILE`s.
3. A real `/trellis:resume <planning-task>` reports
   `Source: session:claude_<session-id>` and keeps `task.json` byte-identical.
4. A second Claude session can select another task without changing the first
   session's `current --source` result.
5. The effective Claude model/provider test is separate from task lifecycle;
   provider success must not be inferred only from hook unit tests.

## 7. Wrong vs Correct

### Wrong

```bash
# This activates implementation and runs hooks; it is not a planning resume.
python3 ./.trellis/scripts/task.py start my-planning-task
```

### Correct

```bash
# This only attaches the current session to the existing planning task.
python3 ./.trellis/scripts/task.py select my-planning-task
python3 ./.trellis/scripts/task.py current --source
```

In OpenCode, prefer the user-facing entry:

```text
/trellis:resume my-planning-task
```

In Claude Code, use the same user-facing entry after automatic SessionStart:

```text
/trellis:resume my-planning-task
```

Do not add a Claude `/trellis:start` alias: it duplicates the native hook and
creates two initialization paths with different session-identity behavior.
