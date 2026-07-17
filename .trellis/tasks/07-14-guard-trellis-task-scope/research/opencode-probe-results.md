# OpenCode equivalent-state probe results

Probe date: 2026-07-15 (Asia/Shanghai)

Environment:

- disposable project: `/tmp/trellis-opencode-model-probe-VOWI4u`;
- OpenCode `1.17.20`, agent `build`, model `krill/grok-4.5`;
- copied official/current Trellis OpenCode integration;
- real project source and task directories were not available to the probe.

## 1. Fresh no-task session

Input requested a one-line README modification and asked OpenCode to follow the
project workflow.

Result:

```text
Trellis SessionStart 上下文已加载。

这是一个很小的改动（README 末尾加一行 `probe`）。本轮要不要创建 Trellis 任务？
```

- no tool call;
- no file mutation;
- correct `no_task` consent behavior.

## 2. Planning session

A disposable planning task was selected for the same OpenCode session. The
user explicitly said implementation was not approved.

Result:

- loaded `trellis-brainstorm`;
- resolved the correct session-scoped task;
- inspected the disposable README and task artifact;
- wrote a lightweight PRD only;
- did not modify README;
- asked the user to review PRD before `task.py start`.

## 3. In-progress session

The disposable task was manually started after adding real JSONL entries.

Result:

- main agent loaded Phase 2.1;
- dispatched `trellis-implement`;
- implement agent received JSONL + PRD context and changed only README;
- main agent loaded Phase 2.2;
- dispatched `trellis-check`;
- check agent verified all disposable PRD acceptance criteria;
- no commit was made.

This proves the configured OpenCode model can follow the unmodified default
Trellis workflow through consent, planning, activation, implement, and check.

## 4. Prompt persistence defect

The first check dispatch persisted an enhanced Task prompt in the parent
conversation. Later check dispatches reused a partial injected prefix, and the
official plugin wrapped it again.

Observed before repair:

- prompt length: 3528 characters;
- `trellis-hook-injected` markers: 2;
- `## Your Context` sections: 2;
- historical production log injections reached 80,539 and 69,105 characters,
  consistent with recursive context growth in long sessions.

The model-reused prefix contained the plugin marker, Context, and Your Task but
omitted the boilerplate Workflow suffix. A complete-template-only unwrapping
rule was therefore insufficient.

## 5. Repair and verification

`inject-subagent-context.js` now:

- detects the plugin-owned marker;
- extracts the caller task body from a complete wrapper;
- accepts the partial marker/context/task prefix observed from OpenCode;
- discards stale duplicated context;
- injects one fresh wrapper from current JSONL/task artifacts.

Verification after repair in the same long-lived OpenCode session:

- debug log: `Unwrapped previously injected Task prompt`;
- prompt length: 2453 characters;
- `trellis-hook-injected` markers: 1;
- `## Your Context` sections: 1;
- implement/check behavior remained successful;
- Node regression suite: 6/6 pass;
- Python task-selection regression suite: 4/4 pass;
- shared workflow SHA-256 remained
  `9f1b573048b85c2e3fd0d099db94bf20e3e09eca983cdd21962ca4c515fdd3f6`.

## 6. Conclusion

The user's Trellis OpenCode installation is correct. The default workflow also
works under equivalent state. The misleading experience came from comparing a
fresh Codex session with a long-lived OpenCode session, amplified by recursive
sub-agent prompt growth. The repair is OpenCode-specific and does not change
shared Trellis policy.

The global contest session-log plugin printed `TEAM_ID not set` inside the
disposable `/tmp` repository. It was non-fatal and unrelated to Trellis; the
probe intentionally did not copy contest credentials into the temporary repo.
