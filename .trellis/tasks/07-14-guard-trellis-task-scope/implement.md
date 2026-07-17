# Implementation Plan: Diagnose OpenCode Trellis workflow adherence

## Preconditions and review gate

- [x] User reviews `prd.md`, `design.md`, and this plan.
- [x] User explicitly approves entering implementation.
- [x] Run `task.py start` only after that approval.
- [x] Load `trellis-before-dev` before editing project integration files.
- [x] Keep `.trellis/workflow.md` unchanged; record its SHA-256 before and
      after all work.

## Step 1 — Record reproducible installation evidence

- [x] Record versions for Trellis CLI/project, OpenCode, local/global
      `@opencode-ai/plugin`, Node, and the configured primary agent/model.
- [x] Compare core `.opencode/lib`, `.opencode/plugins`, and
      `.opencode/agents` files against the official Trellis `0.6.7` npm
      package.
- [x] Separate official matches from modified/deprecated commands and config.
- [x] Save the result under this task's `research/` directory without copying
      secrets or provider credentials.

Validation:

```bash
trellis --version
opencode --version
trellis update --dry-run
sha256sum .trellis/workflow.md
```

Rollback: none; this step is read-only apart from the research report.

## Step 2 — Add cost-free plugin regression probes

- [x] Create a focused Node test/harness that instantiates the official
      OpenCode workflow-state plugin against a temporary Trellis project.
- [x] Cover no-task, planning, and in-progress session records.
- [x] Assert the selected workflow block is prepended exactly once and that
      Trellis sub-agent turns are skipped.
- [x] Add a focused shell-bridge/sub-agent discovery assertion only if existing
      logs are insufficient.

Validation:

```bash
node --test <focused-opencode-plugin-test>
python3 -m unittest discover .trellis/scripts/tests
```

Rollback: remove only the new focused test/harness.

## Step 3 — Build an isolated OpenCode probe project

- [x] Create a disposable `/tmp` Git project with official integration files,
      the unmodified workflow, minimal tasks, and no real source code.
- [x] Ensure the probe cannot resolve a real project task or modify the real
      workspace.
- [x] Capture baseline plugin-log offsets and JSON event output.
- [x] Do not copy provider secrets; let OpenCode use its normal configured
      provider resolution.

Rollback: delete the temporary directory and disposable OpenCode sessions.

## Step 4 — Run equivalent-state model probes

This step invokes the configured provider and may consume quota/cost. Plan
approval authorizes only these narrowly scoped diagnostic calls.

- [x] Fresh session, no task: give a small code-change request and verify the
      first action is task-consent triage, not a tool mutation.
- [x] Continue the same disposable session with a selected planning fixture;
      verify planning routing.
- [x] Continue with an in-progress fixture; verify implement/check dispatch
      intent and Trellis agent discovery.
- [x] Capture the injected breadcrumb and model response for each case.
- [x] If needed, repeat one no-task probe with an alternate supported model via
      one-shot `--model`; do not persist the model change.

Rollback: same as Step 3; no real workspace changes.

## Step 5 — Apply only the proven minimal repair

Choose exactly one branch from `design.md`:

- [ ] Official-file drift → restore through reviewed `.new` sidecars.
- [ ] Session resolution defect → repair OpenCode context-key propagation.
- [ ] Parser/event defect → repair the relevant OpenCode plugin and test it.
- [ ] User-message instruction is ignored but system-layer injection works →
      add a minimal OpenCode-only system-delivery sidecar with deduplication.
- [x] Agent prompt persistence/deduplication defect → unwrap the plugin-owned
      persisted prefix and rebuild one current wrapper.
- [ ] Model-specific limitation → report it; do not change persistent model
      configuration in this task.
- [ ] If no repair branch is proven, make no production/configuration change.

Rollback: execute the branch-specific rollback from `design.md` and rerun the
baseline probe.

## Step 6 — Verification

- [x] Rerun the synthetic plugin tests.
- [x] Rerun the isolated no-task and active-task probes with the configured
      OpenCode model.
- [x] Verify SessionStart deduplication and per-turn state injection logs.
- [x] Verify shell commands resolve the disposable session pointer.
- [x] Verify implement/check context is injected when a Trellis Task dispatch
      occurs.
- [x] Verify `.trellis/workflow.md` SHA-256 is unchanged.
- [x] Run `trellis update --dry-run` and confirm no unintended new drift.
- [x] Run `git diff --check` and inspect only files owned by this task.

## Step 7 — Finish gates

- [x] Run `trellis-check` in Codex inline mode against every acceptance
      criterion.
- [x] Update the OpenCode integration/lifecycle spec only if the diagnosis
      establishes a durable project-specific contract.
- [ ] Present evidence and focused diff before Phase 3.4 commit.
- [ ] Commit and run `/trellis:finish-work` only after the user accepts the
      verified result.
