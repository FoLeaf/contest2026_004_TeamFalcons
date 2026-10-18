# 维护分支来源与集成记录

记录时间：2026-10-18 21:10（Asia/Shanghai）

本文件记录六项维护成果的来源 worktree、来源提交、未提交补丁和
`codex/maintenance-closeout` 中的对应提交。集成采用 cherry-pick 或等价的最小补丁，
因此来源提交不一定是 closeout 的祖先；不能把 `git merge-base --is-ancestor` 的
否定结果解释为成果未集成。实际吸收结果以下面的对应关系、patch-id、文件差异和
集成测试为准。

## 来源现场

| 项目 | worktree | 分支 | 来源提交 | 文件状态 |
|---|---|---|---|---|
| 告警工况 | `.vg-alarm-wt` | `codex/alarm-test-scenarios` | `0e03709` | clean |
| 交付审计 | `.vg-delivery-audit-wt` | `codex/delivery-evidence-audit` | `8c46891`，后续修订 `504d90c` | clean |
| Agent 审查 | `.vg-agent-review-wt` | `codex/agent-fault-review` | `6ba46f0` | `docs/velaguard-agent-tool-review.md` 有用户中断后的补改 |
| MQTT 运行时凭证 | `.vg-d03-wt` | `codex/d03-mqtt-runtime-credentials` | `2b6f929` | `vg_provision.c`、`test_mqtt_cred.c`、`host_tests/Makefile` 有补改 |
| 阶段 D 真机验收 | 原工作区 | `velaguard/maintenance-stability` | `ff140dd` | 保留试读、失联与人工恢复证据 |
| 长测脚本 | `.vg-soak-wt` | `codex/soak-script-validation` | `9648d78` | clean |

来源提交的 patch-id：

| 来源提交 | patch-id |
|---|---|
| `0e03709` | `61f80802f6e3157c95bf7c4be4255126bd281d76` |
| `8c46891` | `34aa93a2aecd961c82b5c8e0caa0d99431bc5f95` |
| `504d90c` | `80b94e6a55661920ba34ba55699312526cea3ed2` |
| `6ba46f0` | `3be7fa638a37432d048b46c60258530cc4962c07` |
| `2b6f929` | `4e398056fe421b8b91c5bb2438db438fac3186e9` |
| `9648d78` | `a5917de6de5bab53b051ca58a7bb0cbcc9ac6f9f` |

## 集成交付关系

| 来源 | closeout 对应提交 | 说明 |
|---|---|---|
| `0e03709` | `9ea9bb7` | 告警场景、模型、headless 检查和脚本整体集成；`01a2b47` 补齐连续步骤滑窗断言 |
| `8c46891` | `8c79a38` | 交付审计初版原样集成 |
| `504d90c` | `c9d8968` | 审计修订建立在 `8c79a38` 之上，不能只取后一个差异提交 |
| `6ba46f0` | `005f525` | Agent 审查、复现和主机用例整体集成；随后以 `375b582`、`17a0f3e`、`85e61d5`、`b86a951` 完成必要修复和回归 |
| `2b6f929` | `9e88868` | MQTT 封存凭证主体、配置和入口改动集成；随后以 `93fedb3`、`b043c61`、`3367192` 完成并发、入口测试和板验边界 |
| `9648d78` | `72656fd` | 长测判定、追溯和离线测试整体集成；随后以 `c3a8dca` 补齐跨串口分块故障识别 |

补充的 `a38c4e5` 来自阶段 D 的候选试读有界等待修复，`eff0fc3` 保存串口失联和
人工恢复现场。两者是本轮 closeout 产生的后续提交，不属于上述来源分支的原始提交。

## 未提交补丁核对

### Agent 审查文档

- 来源 dirty patch-id：`89a6f803c5565306aa223e32cf2be9318144cf33`
- 来源文件 SHA256：`ee1a2451335ab94e9ec04bbde2a9c32cf1a440ca19d14f6cc701ec44c8b9551a`
- 来源补丁有 97 条非空新增行，closeout 版本包含其中 81 条。
- 未直接覆盖的 16 条是后续修复完成后的旧状态文字、旧验收步骤和旧可达性表述；
  其中 `date -s`、`G-03`、`G-04`、`G-05` 等段落已被后续提交替换为更严格的只读
  方案和修复后结论。直接覆盖会把已完成的 F-01 至 F-05 修复回退成审查时状态。
- 结论：保留 closeout 的整合版本，不复制来源 dirty 文件全文。

### MQTT 凭证

- 来源 dirty patch-id：`3f7f11a0a4a5cd5f59a1c2e2b7cd0e6f70dee660`
- `vg_provision.c` 来源 SHA256：
  `7df500f7ea9121eb03e583f54e171c25fa36ab2e93093444fbd34fa21746d99b`
  与 closeout 哈希相同，新增 46 条非空行全部在集成版本中。
- `Makefile` 来源 SHA256：
  `ebb1501269f4f22585e40b4673a60857ec109faa7b7cb93bf390ed33e46bc3aa`
  新增 3 条非空行全部在集成版本中。
- `test_mqtt_cred.c` 来源 SHA256：
  `1bea569dfb30d5c57a3dcd272ac804c82acaa1803398197c66871e51d49be8fb`
  来源补丁有 179 条非空新增行，closeout 版本原样包含 177 条。缺少的两条是对
  `g_mqtt_blob` 私有符号的直接依赖；集成版改用可对修前和修后源码编译的测试桩，
  保留同一确定性交错场景，不降低并发回归的覆盖。
- 结论：来源补丁的必要内容已吸收；没有用来源 dirty 文件覆盖 closeout 中已经过
  修补的测试桩。

## 复核命令与当前状态

来源 worktree 状态复核命令：

```bash
for d in .vg-alarm-wt .vg-delivery-audit-wt .vg-agent-review-wt .vg-d03-wt .vg-soak-wt; do
  git -C "/home/hello19y/openvela/$d" status --short --branch
done
```

2026-10-18 21:10 的结果是告警、交付审计和长测 worktree clean，Agent 和 MQTT
凭证 worktree 仍各有上述未提交补改。集成 worktree 在记录时 clean，分支为
`codex/maintenance-closeout`。

这次记录只证明来源和吸收关系，不代表相关功能、板端验收或 24 小时长测已经完成。
