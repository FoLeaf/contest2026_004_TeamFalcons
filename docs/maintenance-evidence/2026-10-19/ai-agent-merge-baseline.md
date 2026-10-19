# ai_agent 新上游基线合并预览

记录时间：2026-10-19 13:38（Asia/Shanghai）。

## 目的与范围

本地待发布分支 `velaguard/llm-tls-send-retry` 的基线原为
`31faed70f683`。上游 `dev-ai-contest-2026` 已前进到
`e65550f18759`，新增提交只涉及 `src/infra/vela_tls.c`。本次在独立 worktree
中把本地 HEAD `7dc0a8816292` 与上游新基线合并，确认无冲突，并在合并结果上复跑
相关主机测试。本步骤没有推送分支，没有创建 PR，也没有改变开发板上的固件。

## 合并身份

| 项 | 值 |
|---|---|
| 上游父提交 | `e65550f18759f086d7f544edcf17d1e31223244f` |
| 功能分支父提交 | `7dc0a8816292cd58675863e9a1cde8b1cd96a535` |
| 预览合并提交 | `1df3f0aab4635ba204b97fcbb67ea1561ef09378` |
| 合并提交树 | `9487dbf719532376c933b0856fcc0ea89792caa8` |
| 合并时间 | 2026-10-19 13:27:55 +0800 |
| 冲突 | 无 |

预览目录只用于本次验证：

```text
/home/hello19y/vg-pr-preview.QisDcX/ai_agent
```

## 主机回归

在预览合并提交上运行完整参赛仓宿主套件，所有 C 目标通过，
`test_boot_stack.py` 和 `test_board_pins.py` 也单独通过。`/tmp` 下首次运行
`test_vgpoint plain dir` 失败，原因是测试目录位于 tmpfs；改用 home 下的 ext4
预览目录后通过，没有观察到同一失败，因此不改产品代码。

合并后还从测试文件头部列出的命令补跑两个内存边界用例：

```bash
gcc -O2 -g -Wall -Wextra -fstack-protector-all \
  -include tests/stub/host_compat.h \
  -I tests/stub -I include -I src \
  -DCONFIG_EXAMPLES_AI_AGENT_VELA_DATA_DIR='"/tmp/vgtest-skill-summary"' \
  tests/test_skill_summary.c src/tools/skill_loader.c \
  tests/tool_registry_stub.c -o <preview>/test-results/test_skill_summary

gcc -O2 -g -Wall -Wextra -fstack-protector-all \
  -include tests/stub/host_compat.h \
  -I tests/stub -I include -I src \
  -I /home/hello19y/openvela/apps/netutils/cjson/cJSON \
  -DCONFIG_EXAMPLES_AI_AGENT_VELA_DATA_DIR='"/tmp/vgtest-context-prompt"' \
  tests/test_context_prompt.c src/core/context_builder.c \
  src/tools/skill_loader.c tests/tool_registry_stub.c \
  /home/hello19y/openvela/apps/netutils/cjson/cJSON/cJSON.c \
  -o <preview>/test-results/test_context_prompt
```

| 用例 | 结果 | 原始输出 | SHA256 |
|---|---|---|---|
| `test_skill_summary` | 5/5 PASS，退出码 0 | [日志](ai-agent-merge-baseline-skill-summary.log) | `fc9af324baf3e96d77980ab5ff756f16da6ef2ed1fceb096a2b3c69f45e79df0` |
| `test_context_prompt` | 7/7 PASS，退出码 0 | [日志](ai-agent-merge-baseline-context-prompt.log) | `4f2fa183b07fd089b1b7a17f43e2dbf20b83c100d92ae6504ababf78890ee908` |

`test_context_prompt` 首次编译遗漏了用例注释要求的
`-include tests/stub/host_compat.h`，因 glibc 没有 NuttX 的 `OK` / `ERROR`
而失败，未生成可执行文件。补齐该参数后编译无警告并运行通过；归档日志是修正后
的原始输出，不把首次环境错误算作产品回归。

## 结论边界

- 新上游基线合并和各主机回归通过，具备创建 PR 前的代码检查条件。
- 当前公共分支仍待推送，PR 尚未创建。
- 这些是主机结果，不代表上游 CI、CLA、review 或板端重新验收已经完成。
- 合并预览不改变正在运行的 23:48:40 镜像，也不把该镜像的长测结果转给预览提交。
