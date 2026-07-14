# Claude Code + Krill development workflow

## Normal use

Start Claude Code normally from the repository root:

```bash
cd /home/debian19y/openvela/contest2026_004_TeamFalcons
claude
```

Claude Code uses Krill `grok-4.5` globally. Inside this repository, the
project-local `.claude/settings.json` adds Trellis hooks without overriding the
global provider.

The `SessionStart` hook automatically loads the developer, Git, task, workflow,
and specification context. Do not run `/trellis:start`; Claude does not need
the manual fallback used by hook-less platforms.

Use these commands:

```text
/trellis:resume <task-name-or-path>  Select an existing task without starting it
/trellis:continue                    Continue the task selected in this session
/trellis:finish-work                 Finish after quality and commit gates pass
```

`/trellis:resume` is safe for planning tasks. It calls `task.py select`, not
`task.py start`, so it does not change `task.json.status` or run start hooks.

## What was migrated

- Trellis session and workflow-state hooks
- Trellis brainstorm/before-dev/check/break-loop/update-spec skills
- Trellis bundled meta/channel/session-insight/spec-bootstrap skills
- Trellis research/implement/check agents
- planning-safe `/trellis:resume`
- links to the shared openvela build, driver, memory, audio, size, review,
  submission, terminal, and contest-log skills
- the shared openvela driver workflow agent

The existing Claude HUD, status line, permissions, and contest log `Stop` and
`SessionEnd` hooks remain in user settings.

The two Superpowers installations were removed because their brainstorming,
planning, checking, and finish lifecycle duplicated Trellis. Codex-only system
skills, Codex TOML agent definitions, Codex hook syntax, and the redundant
manual `trellis:start` entry were not copied.

## Safe provider verification

This check reports only provider/model structure. It never prints the token:

```bash
node <<'JS'
const fs = require("fs");
const os = require("os");
const path = require("path");
const settings = JSON.parse(
  fs.readFileSync(path.join(os.homedir(), ".claude", "settings.json"), "utf8"),
);
const ok =
  settings.env?.ANTHROPIC_BASE_URL === "https://api.krill-ai.com" &&
  settings.model === "grok-4.5" &&
  settings.env?.ANTHROPIC_MODEL === "grok-4.5" &&
  typeof settings.env?.ANTHROPIC_AUTH_TOKEN === "string" &&
  settings.env.ANTHROPIC_AUTH_TOKEN.length > 0;
console.log(ok ? "Krill configuration: PASS" : "Krill configuration: FAIL");
JS
```

Do not use HTTP debug/body logging to verify authentication. A minimal
`claude -p` request is sufficient for end-to-end validation.

## Configuration ownership

| Path | Owner | Purpose |
| --- | --- | --- |
| `~/.claude/settings.json` | user-local, mode 0600 | Global Krill provider, Claude HUD, permissions, contest hooks |
| `.claude/settings.json` | repository | Trellis hook registration only |
| `.claude/hooks/` | repository/Trellis | Session context, workflow state, sub-agent context |
| `.claude/skills/trellis-*` | repository/Trellis | Claude-native Trellis workflow skills |
| `.claude/skills/<openvela-skill>` | repository symlinks | Shared openvela domain capabilities |
| `.trellis/` | repository | Platform-independent tasks, workflow, specs, and journal |

The Krill credential must never be copied into the repository or a project
local settings file.

## Contest log credential redaction

During migration verification, an exact-byte scan found that seven historical
OpenCode JSONL files contained the Krill credential because the collector's
default rules only recognized `sk-*`, GitHub tokens, and `Bearer` headers.
Those logs were retained, redacted in place, and their `redacted_count`
metadata was updated. Cross-date fragments for two sessions were losslessly
merged by sequence so the official validator could verify the complete logs.

Future collection is protected by generic `apiKey` field rules in two scopes:

| Path | Consumer |
| --- | --- |
| `logs/Foleaf/redact.json` | Repository-local OpenCode collector |
| `~/.claude/contest-collector-staging/Foleaf/redact.json` | Claude Code and Codex staging collector |

The rules contain no credential value. Keep both when reinstalling or moving
the contest collector. Validate collected logs with:

```bash
python3 /home/debian19y/openvela/.claude/skills/contest-log-collector/tools/validate-log.py --quiet logs
```

## Project workflow rollback

To disable the project-local Claude/Trellis adapter without deleting it:

```bash
cd /home/debian19y/openvela/contest2026_004_TeamFalcons
mv .claude ".claude.disabled-$(date +%Y%m%d-%H%M%S)"
ln -s ../.claude .claude
```

This restores the original shared openvela `.claude` view. It does not modify
`/home/debian19y/openvela/.claude`.

To restore the adapter later, remove only the symlink and move the disabled
directory back to `.claude`.

## Global settings recovery

A secret-free structural backup is stored under:

```text
~/.claude/backups/settings-pre-krill-*.sanitized.json
```

It contains compatible non-provider preferences for comparison/recovery but no
provider credential or previous relay values. It is intentionally not a
drop-in provider rollback.

If global settings must be repaired, preserve or re-establish Krill using:

```text
ANTHROPIC_BASE_URL=https://api.krill-ai.com
model=grok-4.5
```

and recover authentication from the existing secure local Krill source without
printing or pasting the token. Never restore opusrelay implicitly from an old
backup.
