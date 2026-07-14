# Technical Design: Claude Code + Krill workflow migration

## 1. Design summary

The migration has two independent configuration scopes but one coordinated
cutover:

1. **User scope** (`~/.claude/`): replace opusrelay with Krill `grok-4.5`,
   remove Superpowers, and preserve compatible Claude-native preferences.
2. **Repository scope** (`<repo>/.claude/`): replace the current shared-skills
   symlink with a real Claude/Trellis adapter, then link only the shared
   openvela capabilities that this repository needs.

No wrapper command is introduced. Running `claude` remains the entry point in
every directory, which makes the provider switch global and the project
workflow automatic.

The work remains one task rather than a parent/child tree because provider
configuration, project hooks, session identity, and end-to-end acceptance must
be cut over and rolled back as one coherent unit.

## 2. Configuration boundaries

### 2.1 User-level provider configuration

`~/.claude/settings.json` remains the single effective global Claude settings
file. Update it atomically while preserving unrelated keys.

Provider/model contract:

```text
ANTHROPIC_BASE_URL=https://api.krill-ai.com
ANTHROPIC_AUTH_TOKEN=<existing Krill bearer token, never printed>
ANTHROPIC_MODEL=grok-4.5
ANTHROPIC_DEFAULT_OPUS_MODEL=grok-4.5
ANTHROPIC_DEFAULT_SONNET_MODEL=grok-4.5
ANTHROPIC_DEFAULT_HAIKU_MODEL=grok-4.5
CLAUDE_CODE_SUBAGENT_MODEL=grok-4.5
settings.model=grok-4.5
```

The base URL deliberately has no `/v1` suffix because Claude Code appends
`/v1/messages`. Remove the old opusrelay URL/model values and the relay-specific
`*_SUPPORTED_CAPABILITIES` overrides; those capability declarations are not a
portable contract for Krill Grok.

The Krill token is read from the already working OpenCode provider entry and
copied in memory into the existing mode-0600 Claude settings file. The
migration process prints only success/failure and structural key names, never
the value. This avoids a wrapper, shell profile dependency, or second plaintext
credential file.

Before replacement, create a mode-0600 **secret-free** structural backup under
`~/.claude/backups/`. It preserves hooks, permissions, status line, and other
non-provider settings while omitting all provider secrets. It is a rollback
aid, not a mechanism for re-enabling opusrelay.

### 2.2 Project-level Claude/Trellis adapter

The existing repository path:

```text
.claude -> ../.claude
```

is replaced by a real project directory. The shared target
`/home/debian19y/openvela/.claude` is not modified.

Use the installed Trellis `0.6.6` Claude configurator with skip-existing
semantics to generate the supported Claude-native files:

```text
.claude/
├── settings.json
├── hooks/
│   ├── session-start.py
│   ├── inject-workflow-state.py
│   └── inject-subagent-context.py
├── commands/trellis/
│   ├── continue.md
│   ├── finish-work.md
│   └── resume.md                 # project-local extension
├── agents/
│   ├── trellis-research.md
│   ├── trellis-implement.md
│   └── trellis-check.md
└── skills/trellis-*/
```

The generated project settings own only Trellis hooks and
`CLAUDE_BASH_MAINTAIN_PROJECT_WORKING_DIR`. Provider credentials remain user
scope. Claude therefore loads global Krill first and adds repository workflow
automation only inside this project.

`CLAUDE.md` remains the Claude instruction entry. Preserve its contest boundary
content and add a compact Trellis pointer so the workflow remains discoverable
even if hooks are deliberately disabled for diagnostics.

## 3. Session identity and task data flow

```text
Claude SessionStart hook input
  └─ session_id / transcript_path
      └─ .claude/hooks/session-start.py
          ├─ resolve_context_key(..., platform="claude")
          ├─ inject current .trellis context
          └─ append TRELLIS_CONTEXT_ID to CLAUDE_ENV_FILE
              └─ later Claude Bash calls inherit the same identity
                  ├─ task.py select <task>
                  ├─ task.py current --source
                  └─ .trellis/.runtime/sessions/<context>.json
```

This is the official Trellis Claude bridge. It solves the earlier degraded
behavior where a hook knew the session ID but a later Bash command did not.
Parallel Claude conversations receive different context keys and therefore
different task pointer files.

The project-local `/trellis:resume <task>` command uses `$ARGUMENTS`, calls
`task.py select`, checks `current --source`, and then routes by the unchanged
task status. It never calls `task.py start`.

Do not add `/trellis:start`. Trellis intentionally omits it on Claude because
Claude has a real SessionStart hook; copying the OpenCode fallback would create
two ways to initialize the same session and would not be behaviorally cleaner.

## 4. Selective migration matrix

| Current capability | Claude destination | Decision | Reason |
| --- | --- | --- | --- |
| `.trellis/` tasks/spec/workflow/journal | same `.trellis/` | Already supplied | Platform-independent source of truth; never copy. |
| `.agents/skills/trellis-*` | `.claude/skills/trellis-*` | Replace with Claude-native generated versions | Same semantics with Claude command references and hook assumptions. |
| `.codex/agents/trellis-{research,implement,check}.toml` | `.claude/agents/*.md` | Replace with Claude-native agents | Claude frontmatter/tool syntax differs from Codex TOML. |
| `.codex/hooks/inject-workflow-state.py` | `.claude/hooks/inject-workflow-state.py` | Replace with Claude-native generated hook | Same workflow source, correct Claude event envelope. |
| Codex manual/session-start skill | Claude `SessionStart` hook | Replace automatically | Claude supports hook injection and shell environment bridging. |
| OpenCode `/trellis:resume` contract | `.claude/commands/trellis/resume.md` | Migrate and adapt | Project-specific planning-safe behavior is not in Trellis 0.6.6 defaults. |
| OpenCode/Codex `trellis:start` fallback | Claude `SessionStart` hook | Drop command | Redundant on a hook-capable platform by Trellis design. |
| Codex global `Stop` contest snapshot | existing Claude `Stop` + `SessionEnd` hooks | Already supplied | Claude collector is already broader; duplicating would export twice. |
| Root `AGENTS.md` | root `CLAUDE.md` + hooks | Adapt pointer only | Preserve contest instructions; Claude uses its native instruction entry. |
| openvela domain skills (`codesize`, drivers, build, etc.) | project `.claude/skills/` symlinks to shared repository | Reuse | Keeps one maintained copy and restores capabilities lost when replacing the top-level symlink. |
| shared driver workflow agent | project `.claude/agents/` symlink | Reuse | Domain agent is not a Trellis agent and remains independently maintained. |
| user skills `design-taste-frontend`, `karpathy-guidelines` | existing `~/.claude/skills/` symlinks | Already supplied | They are already globally visible to Claude; no duplicate migration. |
| user `find-skills` | existing Claude skill, but disabled in Codex | Keep existing Claude state; do not claim as migrated | Not part of the Trellis workflow. |
| Codex system skills (`openai-docs`, plugin installer, etc.) | none | Drop | Product-specific/system-owned and not portable workflow content. |
| Codex MCP servers | none | No action | No Codex MCP configuration exists. |
| Codex provider (`Responses`, `gpt-5.6-sol`) | Krill Anthropic Messages, `grok-4.5` | Replace | User explicitly selected Grok 4.5 for Claude, not the Codex model/API. |
| Claude Superpowers plugins (two copies) | none | Uninstall globally | Duplicate Trellis planning/check/finish and add conflicting SessionStart hooks. |
| Claude HUD plugin/status line | unchanged | Keep | Claude-native UI only; no lifecycle conflict and zero always-on prompt cost. |
| Other existing Claude user skills | unchanged | Keep | Not introduced by this migration and not shown to conflict with Trellis. |
| OpenCode/Codex installations and project adapters | unchanged | Keep as dormant fallback | Removing other tools is not required for a seamless Claude switch. |

## 5. Plugin and settings merge behavior

Uninstall both Superpowers plugin IDs at user scope and remove the extra
Superpowers marketplace. Keep `claude-hud@claude-hud`, its marketplace, and the
current status line. Preserve the official Claude marketplace because it is
Claude-managed and may serve unrelated plugins.

The project Trellis `settings.json` is additive to the user settings. Its hook
events must coexist with user-level contest export hooks:

- project: `SessionStart`, `PreToolUse(Task|Agent)`, `UserPromptSubmit`
- user: `Stop`, `SessionEnd`

No event is duplicated across these two scopes.

## 6. Failure handling and rollback

### Provider update failure

Write the new global settings to a mode-0600 temporary file in
`~/.claude/`, validate JSON and required values, then atomically replace
`settings.json`. If parsing the OpenCode credential or validation fails, do not
replace the original file.

### Project adapter failure

The original `.claude` is only a symlink. If Claude/Trellis generation or
validation fails:

1. move the incomplete real `.claude/` aside for inspection;
2. recreate `.claude -> ../.claude`;
3. leave `/home/debian19y/openvela/.claude` untouched.

### Provider rollback

Rollback restores preserved non-provider settings and hooks, but keeps Krill
or leaves provider fields unset. It must not restore an opusrelay endpoint or
credential from backup. A user who later wants another provider must configure
it explicitly as a new decision.

## 7. Compatibility notes

- Existing Codex/OpenCode task pointers do not become Claude pointers. A new
  Claude session gets its own pointer and uses `/trellis:resume` when attaching
  to an existing task.
- Existing task status and artifacts are shared. Selecting a task is not
  activation.
- Claude Code `--continue`/`--resume` is supported because the SessionStart
  hook runs on startup/compact/clear and the hook-generated environment is
  refreshed for the conversation.
- The shared openvela skills repository remains independently updateable;
  project symlinks observe its updates automatically.
