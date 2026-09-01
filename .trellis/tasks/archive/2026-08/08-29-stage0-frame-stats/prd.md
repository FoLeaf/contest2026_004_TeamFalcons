# 帧级质量统计

> 父任务：`08-29-stage0-foundation`  
> **依赖**：RS485 时序已通过（已归档）  
> 依据：手册 §5.3；推进方案阶段 0。

## Goal

按从站维护滑动窗口通信质量统计，供规则库 / 日后 Agent `get_frame_stats` 使用；stage0 交付可注入、可查询的 MVP（方案 **B**）。

## Confirmed Facts

| 项 | 事实 |
|----|------|
| 挂钩点 | nanoMODBUS：`NMBS_ERROR_CRC` / `TIMEOUT` / 成功路径；`vgmodbus` 轮询可挂接 |
| CRC 统计 | 不重算 CRC；按事务结果计数 `crc_error` |
| 延迟 | 成功事务：请求开始→响应收齐的 ms |
| 代码边界 | `app/velaguard/`（`vg_frame_stats` + 可选 `vgstats` NSH） |

## Decisions（已定）

| 决策 | 选择 |
|------|------|
| MVP 指标 | **B**：CRC 错、超时、延迟 min/max/avg、回声计数；**不含** p50/p95、帧间隔违规（后置） |
| 异常码 | stage0 **不做**直方图（可后续加） |
| 窗口 | 最近 **N 次事务**（默认 64，可配置宏） |
| 查询 | C API + NSH `vgstats dump\|inject\|reset` |
| 持久化 | 仅内存 |

## Requirements

- R1 `vg_frame_stats_*`：按 `slave_id` 记录 / 查询 / 复位
- R2 每次事务上报：`slave`, `ok|crc|timeout|echo|other`, `latency_ms`（失败可 0）
- R3 汇总：总数、各错误计数、成功率、超时率、CRC 率、延迟 min/max/avg（无成功样本时延迟字段无效）
- R4 host 单测：注入序列后断言计数与 avg
- R5 `vgmodbus`（或薄封装）在一问一答后调用 record；不改变 Modbus 语义
- R6 NSH 可 dump / inject（板测）

## Acceptance Criteria

- [x] **AC1** host：`make -C app/velaguard/host_tests` 含 frame_stats 用例通过
- [x] **AC2** 板：`vgstats dump` 在 `vgmodbus` 跑若干轮后非全零；`inject` 可抬升计数（2026-08-30；修复 `vg_fs_init` 重复清零）
- [x] **AC3** 查询只读（dump/get 不写总线）
- [x] **AC4** `research/frame-stats-notes.md`：字段、窗口、注入步骤

## Out of Scope

- p50/p95、帧间隔违规、异常码分布
- Agent Skill / 告警 / MQTT / eMMC 持久化

## References

- `VelaGuard_项目手册.md` §5.3
- `app/velaguard/modbus_collector.c`、`nanomodbus`
