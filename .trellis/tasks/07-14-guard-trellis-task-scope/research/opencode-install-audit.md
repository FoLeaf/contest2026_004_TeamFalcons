# OpenCode Trellis installation audit

Audit date: 2026-07-15 (Asia/Shanghai)

## Versions

| Component | Observed | Reference |
| --- | --- | --- |
| Project Trellis | 0.6.6 | `.trellis/.version` |
| Trellis CLI | 0.6.6 | `trellis --version` |
| Latest npm release audited | 0.6.7 | packed from `@mindfoldhq/trellis@0.6.7` |
| OpenCode executable | 1.17.20 | `opencode --version` |
| Project-local OpenCode plugin package | 1.17.18 | `.opencode/package-lock.json` |
| User-global OpenCode plugin package | 1.17.18 | `~/.config/opencode/package-lock.json` |
| OpenCode primary model | `krill/grok-4.5` | redacted-key review of global config |
| OpenCode primary agent | `build` | global config / `opencode agent list` |
| OpenCode permission default | `allow` | global config / effective agent list |

## Correct installation shape

The official initialization entry is:

```text
trellis init --opencode
```

Expected core runtime surfaces are present:

- `.opencode/plugins/session-start.js`
- `.opencode/plugins/inject-workflow-state.js`
- `.opencode/plugins/inject-subagent-context.js`
- `.opencode/lib/session-utils.js`
- `.opencode/lib/trellis-context.js`
- `.opencode/agents/trellis-{research,implement,check}.md`

## Official 0.6.7 byte comparison

The official npm tarball was unpacked under `/tmp` and compared with `cmp`.

| File | Result |
| --- | --- |
| `lib/session-utils.js` | byte-identical |
| `lib/trellis-context.js` | byte-identical |
| `plugins/inject-subagent-context.js` | byte-identical |
| `plugins/inject-workflow-state.js` | byte-identical |
| `plugins/session-start.js` | byte-identical |
| `agents/trellis-implement.md` | byte-identical |
| `agents/trellis-check.md` | byte-identical |
| `agents/trellis-research.md` | byte-identical |

Conclusion: the executable OpenCode Trellis integration was installed in the
correct layout and is not stale relative to the audited official release.

## Known local differences

- `.opencode/package.json` pins `@opencode-ai/plugin` to exact `1.17.18`;
  official 0.6.7 uses the compatible range `^1.14.39`.
- `.opencode/commands/trellis/continue.md` contains project-local session-safe
  resume guidance.
- `.opencode/commands/trellis/start.md` contains project-local wording and is
  now considered deprecated by `trellis update` because OpenCode has an
  automatic SessionStart plugin.
- `.opencode/commands/trellis/resume.md` is a project-local command for
  status-preserving task selection.

These command differences do not participate in automatic per-turn injection
unless the user explicitly invokes the command.

## Update delta

`trellis update --dry-run` reports:

- CLI 0.6.6 is behind npm 0.6.7.
- generated task/runtime scripts and `workflow.md` have an available template
  refresh;
- the local package and command changes require an explicit merge decision.

The 0.6.7 release notes contain filesystem mutation/state persistence safety
fixes and Pi memory discovery. They do not contain an OpenCode injection or
workflow-adherence repair. Upgrading is sensible maintenance but is not a
supported explanation for the reported behavior.

## Runtime evidence

`/tmp/trellis-plugin-debug.log` confirms that OpenCode:

- loads all three plugins;
- injects SessionStart context once per session and deduplicates later turns;
- injects a workflow-state breadcrumb on each main-session user turn;
- resolves session-scoped task pointers;
- bridges `TRELLIS_CONTEXT_ID` into Bash commands;
- has successfully injected implement/check context into Task tool prompts in
  prior sessions.

The long-lived session `ses_0a175d997ffeqjAFiq2rgAGq1k` moved through several
selected tasks. Comparing that session with a fresh Codex `no_task` session is
not an equivalent-state comparison.

## Instruction-layer difference

The official OpenCode workflow-state plugin prepends the breadcrumb to a user
message via `chat.message`. The installed OpenCode plugin API also exposes
`experimental.chat.system.transform`, but official Trellis does not use that
hook for workflow state. Codex receives its Trellis mode/state through its host
instruction integration. This difference, plus the different model and
permission configuration, is the leading hypothesis after installation drift
was eliminated.

## Audit conclusion

The installation method is not the primary fault. Continue with controlled
fresh-session and active-session probes before changing any integration file.
