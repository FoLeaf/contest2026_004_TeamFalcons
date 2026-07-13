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

## 4. Validation & Error Matrix

| Condition | Required result |
| --- | --- |
| Missing task argument | Non-zero; show command usage |
| Unknown/non-directory task | Non-zero; pointer unchanged |
| Path outside live top-level `.trellis/tasks/` | Non-zero; pointer unchanged |
| Archived task | Non-zero; explain that archived tasks cannot be selected |
| Missing `task.json` | Non-zero; treat as invalid active task |
| Missing session identity | Non-zero; explain supported identity sources |
| Pointer write failure | Non-zero; never print success |
| Valid live task and context key | Zero; report canonical path and session source |

Validation must complete before `set_active_task()` so every error preserves
the caller's existing pointer.

## 5. Good / Base / Bad Cases

- **Good**: a fresh OpenCode window runs `/trellis:resume my-planning-task`;
  `current --source` reports that window's session key, `task.json` remains
  byte-identical, and the next turn injects `(planning)`.
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
