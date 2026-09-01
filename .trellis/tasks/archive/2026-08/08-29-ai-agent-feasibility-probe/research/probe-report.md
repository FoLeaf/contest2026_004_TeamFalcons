# ai_agent H750 可行性探针报告

> 任务：`08-29-ai-agent-feasibility-probe`  
> 日期：2026-08-29  
> 结论：**通过**（AC1–AC4 全过）

## 结论一句话

**openvela `packages/ai_agent` 可以在 STM32H750B-DK（Cortex-M7）上编译、启动、HTTPS 联网，并对大赛 MiMo Token Plan 端点完成一次真实 `ask`。**

## AC 证据

| AC | 结果 | 证据摘要 |
|----|------|----------|
| AC1 | PASS | `bash scripts/build.sh ai-probe --clean --debug`；QSPI 镜像约 621 KB（debug） |
| AC2 | PASS | `nsh> ai_agent` → `vela>`；日志含 `Agent loop started` |
| AC3 | PASS | `net_test` → `Handshake OK: TLSv1.2` → `SUCCESS! HTTP Status: 200` |
| AC4 | PASS | `ask 你好` → `[Agent]: 你好！我是 AI Agent，运行在 Vela 设备上的 AI 助手。...`（~3.9s） |

## 关键配置

- 预设：`stm32h750b-dk:velaguard-ai-probe`
- 构建：`bash scripts/build.sh ai-probe [--clean] [--debug]`
- LLM：`set_llm https://token-plan-cn.xiaomimimo.com/v1 mimo-v2.5 <token>`
- 网络：RJ45 eth0（本次 ICS `192.168.137.x`，出网通后 AC3/AC4 通过）

## 踩坑与修复

1. **烧错固件**：VS Code Build Debug 默认 `TARGET=net`，板端无 `ai_agent`。须显式 `ai-probe`。
2. **`agent_loop` 晚启**：网络 watcher 在 30s 内检测到 eth0 后才 `agent_loop_start`；先确认 `ifconfig` 有 IP 再进 agent。
3. **缺 `/dev/urandom`**：TLS 报 `No secure entropy`。defconfig 增加 `CONFIG_DEV_URANDOM=y`（H750 无 TRNG 驱动时走 xorshift128）。
4. **ICS 出站不稳定**：曾出现 ICMP 通但 TCP 443 超时（`ret=0x44`）；出网恢复后 `net_test`/`ask` 立即成功。
5. **时钟**：板端无 RTC 时 `Clock too old, forcing to 2026`；TLS 仍可完成握手。

## 未做（探针刻意排除）

- eMMC / 持久化 key / Skill / Modbus / LVGL
- 正式 `defconfigs/stm32h750b-dk/` 上游化

## 出口决策

按推进方案 §4.6：**探针通过 → 可开阶段 1 ai_agent 正式集成**（亦可与阶段 0 地基部分并行）。

建议下一任务优先级：

1. 阶段 0 地基（RS485 / 帧统计 / eMMC / 掉电安全存储）中可先做不依赖 Agent 的部分
2. 阶段 1：正式 defconfig + eMMC `/data` + 运营助手 Skill/工具（告警解释 / 日报 / NL 查数）

## 工时

累计约 **2.5–3.5 / 20h**（远低于硬上限）。
