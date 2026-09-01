# ai_agent Cortex-M7 可行性探针（20h 封顶）

> 依据 `VelaGuard_推进方案.md` §4。在任何正式 ai_agent 集成（告警解释 / 报告）之前执行。
> **硬上限 20 小时**——到点未通则停，记录卡点并重排计划。

## Goal

证伪或证实一件事：**openvela `packages/ai_agent` 的运行时能否在 STM32H750B-DK（Cortex-M7）上活下来。**

探针通过是阶段 1（运营助手 Agent 集成）的前置条件；探针失败则评估 `ai_chat` 轻量路径或赛道调整。

## Background

- v3 产品需要板载 ai_agent（告警解释 + 日报 + NL 查数），但 **H750 上无任何 ai_agent 先例**
- 现有 defconfig 仅覆盖 goldfish、Gemini-S1、ESP32-S3
- 前置阻塞项（指定硬件）在探针失败时可不必再问
- 沉没成本低：当前代码库无 ai_agent 集成

## 刻意排除（探针期不做）

- eMMC / FAT 持久化 → 用 RAMMTD + littlefs（掉电丢失可接受）
- API key 持久化 → 编译期 `agent_secrets.h`
- Modbus、VelaGuard 业务工具、Skill Markdown
- LVGL / LTDC UI
- 与探针无关的 velaguard-net 功能扩展（可基于 `velaguard-net` 或独立 `velaguard-ai-probe` 预设）

## Requirements

- R1 新建（或裁剪）H750 defconfig，启用 `CONFIG_EXAMPLES_AI_AGENT_VELA=y` 及推进方案 §4.4 最小 Kconfig 集
- R2 镜像在 WSL 工具链下编译链接通过
- R3 板端启动后 `nsh> ai_agent` 进入 `vela>` 提示符
- R4 `vela> net_test` 通过（依赖 RJ45 网络栈，可复用 velaguard-net 网络基线）
- R5 `vela> ask 你好` 收到 LLM 回复（MiMo 或配置的 OpenAI 兼容端点）
- R6 探针全程累计工时 ≤ 20h；超时强制停并写结论

## Acceptance Criteria

- [ ] **AC1** 带 `CONFIG_EXAMPLES_AI_AGENT_VELA=y` 的镜像编译链接成功
- [ ] **AC2** 启动后 `ai_agent` → `vela>` 提示符
- [ ] **AC3** `vela> net_test` 通
- [ ] **AC4** `vela> ask 你好` 收到非空 LLM 回复
- [ ] **AC5** 探针报告文档：卡在哪一步、已排除什么、堆/栈观测、下一步建议（写入 task `research/probe-report.md`）

**四条 AC1–AC4 全过才算探针通过。**

## Out of Scope

- 正式 `defconfigs/stm32h750b-dk/` 与 eMMC 持久化（阶段 1）
- 自定义 C 工具、Skill、运营助手场景
- 上游 PR（ai_agent H750 支持）——探针通过后再开任务

## 出口决策（探针结束后）

| 结果 | 下一步 |
|---|---|
| AC1–AC4 全过 | 开阶段 1 ai_agent 正式集成任务 |
| 编译/链接失败 | 记录缺失符号/Kconfig；评估 fix 脚本或 ai_chat |
| 启动/栈溢出 | 记录栈落位与堆配置；评估裁剪或 SDRAM 堆 |
| net_test 失败 | 先隔离网络 vs ai_agent；不混查 |
| ask 失败 | 隔离 mbedTLS / DNS / API key / 缓冲落位 |
| 20h 到点未通 | **停**；重排阶段 0/1 或赛道 |

## References

- `VelaGuard_推进方案.md` §4（配置起点、风险表、出口条件）
- `VelaGuard_项目手册.md` §15（前置阻塞项）
- `docs/agents/BOUNDARY.md` C9（探针不替代最终赛道证据，但解锁后续开发）
