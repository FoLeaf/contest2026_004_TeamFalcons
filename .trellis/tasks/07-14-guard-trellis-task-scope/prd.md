# Diagnose OpenCode Trellis workflow adherence

## Goal

Determine why OpenCode appears to bypass Trellis task-creation consent and
implementation/check routing while Codex follows the same default workflow.
Repair only OpenCode-specific installation, configuration, or integration
drift that is proven to cause the behavior. Do not customize the shared
Trellis workflow merely to compensate for a platform-specific problem.

## Background

- Trellis CLI/project version is `0.6.6`; npm currently offers `0.6.7`.
- OpenCode executable version is `1.17.20`.
- Project-local and user-global `@opencode-ai/plugin` are pinned to `1.17.18`.
- The correct Trellis initialization flag is `trellis init --opencode`.
- The project contains all expected OpenCode integration surfaces: three
  plugins, three Trellis agents, Trellis commands, skills, and supporting
  libraries.
- Every core `.opencode/lib`, `.opencode/plugins`, and `.opencode/agents`
  file is byte-identical to the official Trellis `0.6.7` npm templates.
  This is strong evidence that the core installation was not copied or
  generated incorrectly.
- The current `.trellis/workflow.md` state blocks are also semantically
  identical to the official `0.6.7` native workflow. Codex and OpenCode are
  therefore receiving policy from the same workflow source.
- OpenCode successfully loads all three plugins, injects SessionStart context,
  injects a per-turn workflow breadcrumb, bridges its session ID into shell
  commands, and has historically injected implement/check sub-agent context.
- The active OpenCode session is long-lived and has selected several different
  Trellis tasks. Once a task is selected, `no_task` routing no longer applies;
  this differs from the fresh Codex session used for comparison.
- OpenCode injects the workflow breadcrumb by prepending text to the user
  message. Codex receives Trellis state through its host integration at a
  higher instruction layer. Model adherence can therefore differ even when
  workflow text is identical.
- The global OpenCode configuration uses `krill/grok-4.5`, default agent
  `build`, and `permission: allow`; Codex is not using the same host/model/
  permission combination.
- OpenCode conversation history is stored in SQLite and cannot currently be
  inspected by `trellis mem`, so conclusions must rely on runtime state,
  plugin logs, controlled probes, and task artifacts.

## Requirements

### R1 — Preserve the shared default workflow

- Do not edit `.trellis/workflow.md` as part of this task.
- Do not modify Trellis's global npm installation or package cache.
- Do not propose an upstream default-workflow change unless a controlled test
  reproduces the defect across platforms with equivalent state and instruction
  conditions.

### R2 — Verify installation and version integrity

- Compare every executable OpenCode Trellis integration file with the matching
  official release template.
- Identify locally modified/deprecated OpenCode files separately from core
  plugins and agents.
- Determine whether the exact `@opencode-ai/plugin` pin or the `1.17.18` versus
  `1.17.20` patch mismatch is supported and materially relevant.
- Review the `0.6.7` update/migration delta using non-destructive preview or
  sidecar files before applying anything.

### R3 — Compare equivalent runtime states

- Do not compare a fresh Codex `no_task` turn with an OpenCode session that
  already has an active `planning` or `in_progress` task.
- Verify OpenCode behavior in a fresh session with no selected task.
- Verify behavior in a fresh session after selecting one known planning task.
- Distinguish these failure classes:
  1. state resolution selects the wrong task;
  2. the correct breadcrumb is not injected;
  3. the breadcrumb is injected but the main model ignores it;
  4. the main model follows task routing but skips implement/check dispatch;
  5. a user/session override intentionally bypasses Trellis for the turn.

### R4 — Minimal OpenCode-only repair

- Prefer restoring or upgrading official generated files over local policy
  customization.
- If the files and versions are correct but instruction adherence differs,
  repair the OpenCode-specific instruction delivery or main-agent
  configuration without changing shared workflow semantics.
- Preserve working session isolation, shell bridging, sub-agent context
  injection, and existing task selection/resume behavior.
- Keep a simple rollback path for every configuration change.

### R5 — Preserve explicit user control

- Pure conversation and read-only diagnosis must not mutate task state.
- A controlled probe must not silently create or start a real project task.
- Any external model call that could incur cost or consume provider quota must
  be visible in the implementation plan and approved with that plan.

## Acceptance Criteria

- [x] Core installation integrity is reported file-by-file against an official
      Trellis release.
- [x] Modified or deprecated OpenCode commands/config files are identified and
      shown not to be confused with core plugin installation.
- [x] Equivalent fresh-session and active-task cases establish exactly where
      Codex and OpenCode behavior diverges.
- [x] The diagnosis identifies whether the cause is session state, plugin
      loading, instruction delivery, model adherence, agent dispatch, or a
      version mismatch.
- [x] If a repair is needed, it is OpenCode-specific, minimal, reversible, and
      leaves `.trellis/workflow.md` unchanged.
- [x] SessionStart, per-turn workflow-state, shell session bridge, and
      implement/check context injection continue to work after the repair.
- [x] Re-dispatching an already persisted/enhanced OpenCode Task prompt
      produces exactly one Trellis wrapper and one current copy of each
      PRD/spec/task body instead of recursively nesting prior context.
- [x] A fresh OpenCode session with no task handles task-creation consent as
      defined by the unmodified default workflow.
- [x] An active-task OpenCode session follows the unmodified planning or
      implementation routing appropriate to that task.
- [x] No real task is created/started and no provider call is made solely for
      testing without explicit plan approval.

## Out of Scope

- Editing the native `.trellis/workflow.md` task policy.
- Changing Trellis upstream default workflow templates.
- Completing project coding specs or cleaning unrelated active tasks.
- Implementing the missing OpenCode SQLite adapter for `trellis mem`.
- Replacing the user's OpenCode provider/model unless testing proves it is the
  cause and the user separately approves that configuration change.

## Confirmed Decision

- The user does not want a project-local or upstream default-workflow rewrite;
  the task must first treat this as an OpenCode-specific installation or
  integration problem.

## Notes

- This is a complex diagnostic/repair task. Add `design.md` and `implement.md`
  before requesting implementation approval.
