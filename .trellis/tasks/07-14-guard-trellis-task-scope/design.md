# Technical Design: OpenCode Trellis workflow adherence diagnosis

## 1. Decision

Treat the reported behavior as an OpenCode-specific integration/adherence
problem until an equivalent-state test proves otherwise. Keep the shared
native workflow byte-unchanged.

The investigation proceeds through four boundaries in order:

```text
official template integrity
  -> runtime task-state resolution
  -> instruction delivery to the main model
  -> model routing / sub-agent dispatch behavior
```

A later boundary is not changed merely because an earlier one is easier to
patch. In particular, do not rewrite task policy before proving that the
OpenCode plugin delivered the correct policy and state.

## 2. Confirmed architecture

### 2.1 Shared semantic source

`.trellis/workflow.md` owns the task policy and workflow-state bodies. Codex
and OpenCode read the same file.

### 2.2 OpenCode delivery path

OpenCode auto-discovers `.opencode/plugins/*.js`:

1. `session-start.js` prepends compact Trellis context to the first user
   message and deduplicates it for later messages.
2. `inject-workflow-state.js` resolves the OpenCode session key, reads the
   selected task, parses the matching workflow-state block, and prepends the
   breadcrumb to every main-session user message.
3. `inject-subagent-context.js` injects task/spec context before a Trellis Task
   tool dispatch and bridges the OpenCode session ID into Bash commands.

The per-turn workflow breadcrumb is therefore delivered as user-message text,
not as an OpenCode system instruction.

### 2.3 Codex delivery path

Codex uses its platform hook/instruction integration and this project is in
`codex.dispatch_mode=inline`. The comparison is not equivalent when Codex has
`no_task` while OpenCode has a selected `planning`/`in_progress` task, or when
the two hosts use different models and instruction priority.

## 3. Diagnostic matrix

| Probe | State | Evidence expected | Failure classification |
| --- | --- | --- | --- |
| Static template audit | N/A | Core OpenCode files equal official package | Installation drift if unequal |
| Synthetic plugin probe | `no_task` | Exact `no_task` breadcrumb prepended | Plugin/parser defect if absent |
| Synthetic plugin probe | planning/in-progress | Correct task ID/status block | Session resolver defect if wrong |
| Fresh OpenCode model probe | `no_task` | Model asks for task consent before mutation | Instruction/model adherence if breadcrumb exists but ignored |
| Active-task model probe | selected dummy task | Model follows matching phase routing | State/adherence issue if it does not |
| Dispatch probe | dummy in-progress task | Main agent attempts `trellis-implement`/check flow | Agent/tool routing issue if skipped |

Static and synthetic probes are local and cost-free. Model probes use the
configured provider and can incur quota/cost; they run only after the user
approves the final implementation plan.

## 4. Isolated probe environment

Do not test routing by creating or starting another real project task. Build a
temporary project under `/tmp` containing only:

- the official/current `.opencode` integration files;
- a minimal `.trellis/workflow.md` copy;
- minimal task fixtures and session runtime records;
- a disposable Git repository;
- no project source code or credentials.

Run OpenCode with the same `build` agent/model settings as the reported
environment. Capture JSON events and Trellis plugin debug lines. Use a unique
debug log or take before/after offsets so unrelated live-session events cannot
contaminate results.

For active-state probes, obtain the fresh OpenCode session ID from the first
JSON event, create the matching disposable session pointer in the temporary
project, and continue only that session. All task mutations remain inside the
temporary directory.

## 5. Repair decision tree

### A. Template or dependency drift

If executable integration files differ from the official template, restore
them through `trellis update --create-new` review or a scoped `trellis init
--opencode` refresh. Never overwrite modified commands blindly.

### B. Wrong session state

If OpenCode resolves the wrong task, repair only session-key propagation or
stale pointer cleanup. Preserve the current per-session storage contract.

### C. Breadcrumb missing or malformed

Repair the OpenCode parser/event hook and add a synthetic regression test. The
workflow body remains unchanged.

### D. Breadcrumb correct but ignored

First compare the same model with the breadcrumb delivered through OpenCode's
`experimental.chat.system.transform` hook. This hook can append a dynamic
system instruction, while the current official plugin modifies user-message
text. If and only if the system-layer probe fixes adherence, implement a
minimal OpenCode-specific delivery change with deduplication and no duplicate
visible breadcrumb.

Because the hook is marked experimental, prefer a small project-local wrapper
or sidecar and retain the official plugin for rollback. Do not silently alter
global OpenCode configuration.

### E. Correct routing but missing sub-agent dispatch

Verify the main agent can see the `trellis-*` agents and Task tool, and that
the dispatch plugin recognizes the selected task. Repair agent discovery or
tool hook matching, not workflow policy.

### F. Model-specific non-adherence

If official injection works with another supported model but not the configured
`krill/grok-4.5`, report that result. Do not replace the user's provider/model
without separate approval.

## 6. Version and local-change handling

- Trellis `0.6.7` contains filesystem-safety fixes, not an OpenCode injection
  fix. Upgrading is useful maintenance but not a presumed cure.
- Core OpenCode plugin/lib/agent files already match official `0.6.7`.
- `.opencode/package.json`, `continue.md`, and deprecated `start.md` contain
  local changes. Preserve them during any update and review `.new` sidecars.
- The exact plugin pin (`1.17.18`) differs from the official semver range
  (`^1.14.39`) but matches the installed package. Treat it as a hypothesis only
  if a compatibility probe fails.

## 7. Rollback

- Temporary probes: delete only their `/tmp` directory and test sessions.
- Restored generated file: keep a pre-change hash/copy and restore it exactly.
- OpenCode sidecar/system-delivery experiment: remove the sidecar and restart
  OpenCode; the official user-message plugin remains available.
- Model/config experiment: pass model/agent on the one-shot CLI only; do not
  persist global config changes.

No rollback path changes `.trellis/workflow.md`.

## 8. Proven repair branch

The equivalent-state probes eliminated template drift, state resolution,
breadcrumb delivery, model adherence, and agent discovery as causes:

- fresh `no_task` asked for task-creation consent before any tool call;
- planning loaded `trellis-brainstorm`, wrote only a PRD, and requested review;
- in-progress dispatched `trellis-implement` and then `trellis-check` with the
  correct session-scoped task context.

The probe did expose an OpenCode-only prompt persistence defect. The plugin
mutates Task args in place so the enhanced prompt persists in the parent
conversation. On later dispatches the main model can reuse a full or partial
`trellis-hook-injected` prefix. The official plugin wrapped that prefix again,
duplicating PRD/spec context and allowing recursive prompt growth.

The selected repair makes injection idempotent:

1. Detect the plugin-owned marker.
2. Recover the caller-authored body after `## Your Task`.
3. Support both the complete plugin template and the partial prefix observed
   from the OpenCode main model.
4. Rebuild one wrapper using current task/spec files.
5. Leave malformed third-party prompts without the exact owned shape intact.

This changes only `.opencode/plugins/inject-subagent-context.js` and leaves the
shared workflow unchanged.
