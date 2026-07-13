# Bug Analysis: OpenCode could not resume an existing planning task

## 1. Root Cause Category

- **Category**: B — Cross-Layer Contract, with A — Missing Spec as a
  contributing factor.
- **Specific cause**: session initialization, session-scoped task selection,
  and implementation activation were represented by similar “start/resume”
  language across the OpenCode command layer, Trellis CLI, workflow text, and
  runtime pointer store. The system had no public operation that performed
  selection alone. `/trellis:start` only loaded context, while `task.py start`
  also changed `planning` to `in_progress` and ran hooks.

## 2. Why Earlier Fixes Failed

1. **Repeated `/trellis:start`**: reloaded the same `no_task` context but never
   wrote a session pointer.
2. **Considering `task.py start`**: would have hidden the symptom by advancing
   lifecycle state without planning approval.
3. **Prompt-only guidance**: could explain what to do but had no safe public
   command through which to persist the pointer.
4. **Initial non-interactive acceptance attempts**: used the TUI-style nested
   command name with `opencode run --command`. OpenCode 1.17.18's raw CLI uses
   the file-relative name `trellis/resume`; the control request proved the
   Provider was healthy before the corrected invocation passed.

## 3. Prevention Mechanisms

| Priority | Mechanism | Specific action | Status |
| --- | --- | --- | --- |
| P0 | Architecture | Add hook-free, status-preserving `task.py select` | DONE |
| P0 | Runtime safety | Fail selection when session identity is unavailable | DONE |
| P0 | Integration entry | Add OpenCode `/trellis:resume <task>` | DONE |
| P1 | Documentation | Define initialization, selection, and activation separately | DONE |
| P1 | Test coverage | Add unit tests for state preservation, validation, hooks, and bridge matching | DONE |
| P1 | End-to-end | Verify a fresh OpenCode session receives a planning breadcrumb on its next turn | DONE |

## 4. Systematic Expansion

- **Similar issues**: other AI platforms may lack a planning-safe resume entry
  even though the core session-pointer API exists. That is an upstream/all-
  platform follow-up, not part of this project-local repair.
- **Design improvement**: lifecycle mutation belongs only to `start`; pointer
  selection is an independent primitive. Commands must call the primitive that
  matches their declared state transition.
- **Process improvement**: acceptance for session integrations must include a
  second turn. A successful pointer write alone does not prove the platform
  hook resolves and injects the same context key.
- **Knowledge gap**: OpenCode TUI slash naming and `opencode run --command`
  raw command naming are different surfaces and must be tested separately.

## 5. Knowledge Capture

- [x] Added `.trellis/spec/backend/trellis-session-task-lifecycle.md`.
- [x] Added persistent regression tests under `.trellis/scripts/tests/`.
- [x] Updated OpenCode start/continue/resume guidance.
- [x] Updated `.trellis/workflow.md` lifecycle terminology.
- [ ] Consider upstream/all-platform resume support in a separate task if the
      same need appears outside OpenCode.
