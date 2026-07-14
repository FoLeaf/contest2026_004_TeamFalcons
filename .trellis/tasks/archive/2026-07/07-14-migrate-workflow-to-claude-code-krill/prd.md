# Migrate workflow to Claude Code with Krill Grok 4.5

## Goal

Allow the developer to switch from Codex/OpenCode to Claude Code and continue
the same Trellis-managed work without relearning the workflow. Claude Code must
use Krill `grok-4.5` through `https://api.krill-ai.com` globally, with no
remaining dependency on opusrelay, while credentials and unrelated
platform-specific customizations remain safe.

## Background

- Claude Code `2.1.208` and Trellis `0.6.6` are installed in WSL. Claude Code
  began this migration at `2.1.199` and auto-updated during interactive
  verification; the migrated workflow passed on the updated version.
- Krill accepts Anthropic Messages requests at `/v1/messages`; a request using
  model `grok-4.5` has already returned HTTP 200.
- OpenCode currently uses Krill at `https://api.krill-ai.com/v1` with model
  `krill/grok-4.5`. Its credential is stored locally and must never be printed,
  pasted into chat, or committed.
- Claude Code currently uses a user-level `~/.claude/settings.json` configured
  for `https://opusrelay.com`. The user has decided to replace that provider
  globally with Krill and discard opusrelay. The file also owns contest log
  export hooks, the existing status line, permissions, and unrelated user
  preferences that must survive the provider change when still applicable.
- The repository's `.claude` path is currently a symlink to
  `/home/debian19y/openvela/.claude`. That target provides shared openvela
  domain skills but no project Trellis settings, hooks, commands, or agents.
- The current Trellis state, tasks, specs, workflow, and journal already live
  under this repository's `.trellis/` directory and are platform-independent.
- The planning-safe task handoff contract is `task.py select <task>`: selecting
  a task must not start it, change its status, or run task lifecycle hooks.
- User-level Claude settings still enable two Superpowers plugin entries.
  Superpowers lifecycle/brainstorming behavior must not compete with Trellis.

## Requirements

### R1. Project-owned Claude Code workflow entry

Claude Code launched from this repository must discover a project-owned
Trellis integration rather than writing repository-specific Trellis files into
the shared `/home/debian19y/openvela/.claude` skills repository.

### R2. Global Krill provider and model

Claude Code sessions globally must use:

- `ANTHROPIC_BASE_URL=https://api.krill-ai.com`
- model `grok-4.5` for the main session and Trellis sub-agents
- Anthropic Messages-compatible bearer authentication accepted by Krill

The previous opusrelay URL, models, and token must be removed from the effective
Claude configuration rather than kept as a fallback.

### R3. Credential safety

Reuse or migrate the existing Krill credential without displaying it. No
credential may appear in tracked files, shell output, task artifacts, logs, or
chat. Any credential-bearing file must be outside Git, restricted to the
current user, and consumable automatically by Claude Code.

### R4. Trellis parity

A fresh Claude Code session must automatically load the repository's Trellis
context and expose the Claude equivalents of the current workflow:

- session-start context and per-turn workflow-state breadcrumb
- brainstorm, before-dev, check, update-spec, break-loop, continue/start, and
  finish-work behavior
- `trellis-research`, `trellis-implement`, and `trellis-check` agents with task
  artifact/spec injection or an explicit fallback loading path
- the same `.trellis/tasks/`, `.trellis/spec/`, `.trellis/workspace/`, and
  `.trellis/workflow.md` state used by Codex/OpenCode

### R5. Planning-safe task resume

Claude Code must provide a `/trellis:resume <task>` entry point equivalent to
the existing OpenCode command. It must select the requested task for the
current Claude session without changing `planning` to `in_progress` and without
running start hooks. `/trellis:continue` must continue to mean “resume the
current task at its existing phase,” not “select an arbitrary task.”

### R6. Session isolation

Task selection/current-task resolution must be scoped to the active Claude
conversation. A fresh or parallel Claude session must not silently inherit a
different session's selected task. If Claude Code does not expose its session
identity to Bash commands directly, the Claude integration must bridge the
hook-provided session identity to `task.py` in a tested way.

### R7. Selective capability migration

Inventory the current Codex workflow surfaces—skills, agents, hooks,
instructions, commands, MCP/plugin-like integrations—and classify each as
“migrate,” “replace with Claude-native equivalent,” “already supplied,” or
“drop.” Retain access to shared openvela domain skills and agent definitions
needed by this project without duplicating or forking their content. Codex
internals, Codex-only system skills, duplicated entry points, stale experiments,
and unrelated integrations must not be copied merely because they exist.

### R8. Single planning owner

Trellis remains the only planning/execution/check/finish lifecycle. Remove or
disable the currently enabled Superpowers plugins globally so they cannot
reintroduce a second brainstorming or lifecycle system. Claude-native UI
support such as the existing status line may remain if it does not compete with
Trellis.

### R9. Preserve unrelated Claude behavior

The current contest log `Stop` and `SessionEnd` hooks, permissions, status line,
and other compatible Claude-native preferences must remain functional unless a
specific incompatibility is demonstrated. Provider replacement must not erase
them accidentally.

### R10. Reversible migration

Create a secret-free backup of the configuration structure needed for rollback
and document how to restore the original project `.claude -> ../.claude`
symlink. Rollback may restore configuration shape and hooks, but must not
silently reactivate discarded opusrelay credentials.

## Acceptance Criteria

- [ ] AC1: From this repository and from an unrelated directory, fresh Claude
  Code sessions report model `grok-4.5` and complete a minimal request through
  Krill.
- [ ] AC2: The effective Claude settings contain no opusrelay endpoint or
  opusrelay model mapping, and Krill is the global default.
- [ ] AC3: No tracked file or captured command output contains the Krill token;
  its credential storage is outside Git and has user-only permissions.
- [ ] AC4: A fresh Claude Code session receives contest/Trellis context and a
  workflow breadcrumb without manually pasting a prompt.
- [ ] AC5: Claude Code lists and can invoke the Trellis commands, workflow
  skills, and three Trellis agents, while required shared openvela domain
  skills remain discoverable.
- [ ] AC6: `/trellis:resume <planning-task>` selects an existing planning task
  in a test Claude session while its `task.json` remains byte-identical and no
  start hook runs.
- [ ] AC7: Two concurrent/fresh Claude sessions resolve their own task pointer
  rather than sharing a single accidental active task.
- [ ] AC8: Superpowers brainstorming/lifecycle behavior no longer loads in
  Claude Code, and Trellis remains the single workflow-state source.
- [ ] AC9: Existing user-level contest-log hooks, permissions, and status line
  still work after the provider and workflow migration.
- [ ] AC10: The documented rollback restores the original project `.claude`
  symlink without modifying the shared openvela `.claude` repository and
  without silently re-enabling opusrelay.
- [ ] AC11: A written migration matrix accounts for every relevant current
  Codex workflow skill/agent/hook/integration and demonstrates that no required
  capability was omitted or duplicated.

## Out of Scope

- Changing or publishing the Trellis npm package.
- Rotating or revealing the Krill credential.
- Removing shared openvela skills from their existing repository.
- Reworking firmware, VS Code, build, flash, or debug configuration.
- Reproducing Codex product internals or system-only skills inside Claude Code.
