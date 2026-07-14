# Implementation Plan: Claude Code + Krill workflow migration

## Preconditions and review gate

- [ ] User reviews `prd.md`, `design.md`, and this plan.
- [ ] Only after explicit implementation approval, run:

  ```bash
  python3 ./.trellis/scripts/task.py start \
    07-14-migrate-workflow-to-claude-code-krill
  ```

- [ ] Load `trellis-before-dev` before changing project or user configuration.
- [ ] Record the initial `git status`, `.claude` symlink target, Claude/Trellis
  versions, plugin list, and a secret-redacted settings structure.

## Step 1. Create safe rollback material

- [ ] Verify `~/.claude/settings.json` is owned by the current user and mode
  `0600`; correct the mode before reading credentials if needed.
- [ ] Create a timestamped, mode-0600, secret-free structural backup under
  `~/.claude/backups/`. Preserve hooks, permissions, status line, plugin state,
  and non-provider preferences; omit auth values and opusrelay provider values.
- [ ] Verify the repository `.claude` is exactly a symlink to `../.claude` and
  that `/home/debian19y/openvela/.claude` is clean/unmodified by this task.
- [ ] Capture the rollback commands in
  `docs/agents/CLAUDE_CODE_KRILL_MIGRATION.md` without credentials.

Rollback point: no effective configuration has changed yet.

## Step 2. Switch global Claude provider atomically

- [ ] Use a non-logging local script/process to parse
  `~/.config/opencode/opencode.jsonc` and read
  `provider.krill.options.apiKey` in memory. Do not echo it, pass it as a shell
  argument, or write it to the repository.
- [ ] Load `~/.claude/settings.json`, preserve unrelated keys, and update the
  provider/model contract from `design.md`.
- [ ] Remove opusrelay model mappings and relay-specific supported-capability
  variables. Do not keep opusrelay as fallback.
- [ ] Write and validate a temporary mode-0600 JSON file, then atomically
  replace `~/.claude/settings.json`.
- [ ] Run a structural validator that prints only pass/fail and confirms:

  - base URL is exactly `https://api.krill-ai.com`;
  - every main/default/sub-agent model is `grok-4.5`;
  - auth token is present but never printed;
  - no effective value contains `opusrelay`.

Rollback point: restore the secret-free non-provider structure while retaining
Krill or clearing provider keys; never restore opusrelay silently.

## Step 3. Remove conflicting Claude plugins

- [ ] Uninstall both user-scope Superpowers installations with the Claude CLI:

  ```bash
  claude plugins uninstall superpowers@claude-plugins-official --scope user --yes
  claude plugins uninstall superpowers@superpowers-marketplace --scope user --yes
  ```

- [ ] Remove the `superpowers-marketplace` marketplace if no other installed
  plugin depends on it.
- [ ] Verify `claude-hud@claude-hud` remains installed/enabled and its status
  line configuration is unchanged.
- [ ] Verify user `Stop` and `SessionEnd` contest hooks still exist.

Rollback point: plugins may be reinstalled separately if ever desired, but not
as part of provider rollback because Trellis is the selected lifecycle owner.

## Step 4. Materialize the project-local Claude/Trellis adapter

- [ ] Remove only the repository `.claude` symlink; do not delete or modify its
  target.
- [ ] Create an empty real `.claude/` directory.
- [ ] Generate the official Claude adapter using the installed Trellis version
  with non-destructive/skip-existing semantics:

  ```bash
  trellis init --claude --skip-existing --yes \
    --user Foleaf --no-monorepo
  ```

- [ ] Inspect the resulting diff to ensure existing `.trellis/`, `AGENTS.md`,
  tasks, specs, workflow, and journal content were not overwritten.
- [ ] Add relative symlinks for the required shared openvela domain skills and
  `driver-workflow.agent.md`; verify every link resolves inside
  `/home/debian19y/openvela/.claude`.
- [ ] Add project-local `.claude/commands/trellis/resume.md` with Claude
  `$ARGUMENTS` syntax and the planning-safe `task.py select` contract.
- [ ] Add a compact Trellis pointer to `CLAUDE.md` while preserving all contest
  boundary instructions.
- [ ] Update `.gitignore` only if a Claude local/runtime file produced during
  verification is not already safely ignored. Do not ignore tracked workflow
  files or contest logs.

Rollback point:

```bash
mv .claude .claude.failed-<timestamp>
ln -s ../.claude .claude
```

## Step 5. Validate files and migration matrix

- [ ] Confirm generated Claude settings register:

  - `SessionStart` for startup/clear/compact;
  - `PreToolUse` for Task/Agent sub-agent context;
  - `UserPromptSubmit` for workflow-state injection.

- [ ] Confirm Claude commands include `continue`, `finish-work`, and the local
  `resume`, but not redundant `start`.
- [ ] Confirm the five Trellis workflow skills, four bundled skills, three
  agents, shared openvela domain skills, and shared driver agent are present.
- [ ] Confirm user-global `design-taste-frontend` and `karpathy-guidelines`
  symlinks still resolve.
- [ ] Confirm the migration matrix in `design.md` still matches the filesystem;
  revise it if implementation evidence differs.
- [ ] Run Trellis task/runtime tests relevant to session selection:

  ```bash
  python3 -m unittest .trellis/scripts/tests/test_task_select.py
  ```

  If test discovery requires a different module path, use the repository's
  existing unittest invocation without altering test semantics.

## Step 6. Validate credential safety

- [ ] Assert `~/.claude/settings.json` and the structural backup are mode 0600.
- [ ] Read the Krill token into memory and scan `git ls-files` contents for an
  exact byte match; print only pass/fail, never the token or matching content.
- [ ] Confirm no repository local settings or task artifacts contain auth keys.
- [ ] Confirm shell history/debug output was not used to print the credential.

## Step 7. Validate provider from two scopes

- [ ] From a temporary unrelated directory, run a minimal non-interactive
  Claude request and parse only safe result metadata. Assert request success and
  response model `grok-4.5`.
- [ ] From this repository, repeat the request. Assert Krill succeeds with the
  project Trellis settings loaded.
- [ ] Do not enable HTTP body/header debug logging during either test.
- [ ] Verify the effective settings scan still contains no opusrelay value.

## Step 8. Validate Trellis hooks and per-session resume

- [ ] Invoke the generated Claude `session-start.py` with two synthetic Claude
  session IDs and separate temporary `CLAUDE_ENV_FILE`s.
- [ ] Verify each env file receives a different `TRELLIS_CONTEXT_ID` and that
  the hook output contains the expected Trellis session context/breadcrumb.
- [ ] In session A, execute `/trellis:resume` against an existing task whose
  status is still `planning` (use
  `07-13-resolve-usart3-rs485-vcp-conflict` if it is still suitable) and verify:

  - `task.py current --source` reports the selected session pointer;
  - the task JSON is byte-identical before/after selection;
  - status remains `planning`;
  - no `after_start` hook ran.

- [ ] In session B, select the migration task (which is expected to be
  `in_progress` by then) and verify session A still resolves its planning task.
  Remove only the synthetic runtime pointers after the assertion.
- [ ] Start a fresh real Claude Code session from the repository and ask it to
  report the injected task/status/next phase. Confirm it does not load
  Superpowers and can discover `/trellis:resume` plus the Trellis agents.

## Step 9. Documentation, quality gate, and handoff

- [ ] Complete `docs/agents/CLAUDE_CODE_KRILL_MIGRATION.md` with:

  - normal use (`claude`, automatic SessionStart, `/trellis:resume`,
    `/trellis:continue`, `/trellis:finish-work`);
  - provider/model verification without revealing credentials;
  - migration matrix summary;
  - project-adapter rollback and secret-safe provider recovery.

- [ ] Run `trellis-check` in inline mode over project changes and rerun failed
  validations after fixes.
- [ ] Run `trellis-update-spec` to extend the task lifecycle spec with the
  Claude `/trellis:resume` entry and SessionStart environment bridge.
- [ ] Review `git diff` and ensure unrelated dirty user changes remain intact.
- [ ] Present verification results and request final acceptance before the
  normal Phase 3 commit/archive flow.
