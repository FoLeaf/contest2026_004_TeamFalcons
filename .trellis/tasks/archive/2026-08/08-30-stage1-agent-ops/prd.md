# 运营助手 Skill + 主动任务

> 父任务：`08-30-stage1-min-product`  
> **依赖**：`08-30-stage1-ai-agent`（eMMC `/data/agent`、run_shell 白名单）  
> **口径**：`08-25-agent-capability-rescope/design.md`

## Goal

交付大赛 C9「主动+执行」最小证据：**告警解释（事件主动）** + **运营日报（定时/心跳主动）**；全部只读，不写寄存器。

## Requirements

- R1 种子 Skill：`alarm_interpretation.md`、`operations_report.md`（eMMC `/data/agent/skills/`）
- R2 种子 `HEARTBEAT.md`：指引 Agent 处理 pending 告警与缺失日报
- R3 本地告警探测：`vg_agent_alarm` 线程监测从站离线（帧统计）与超阈值（Modbus 只读）
- R4 pending 告警落盘：`/data/velaguard/pending_alarm.txt`；报告目录 `/data/velaguard/reports/`
- R5 可选 `CONFIG_VG_AGENT_AUTOSTART`：后台拉起 `ai_agent`（cron/heartbeat 需 Agent 进程）
- R6 `bash scripts/build.sh net` 编译通过

## 刻意排除

- 规则引擎 / 485 归因（阶段 2）
- LVGL 展示（`stage1-lvgl-hmi`）
- MQTT 推送解释摘要（可选后续）

## Acceptance Criteria

- [x] **AC1** 编译链接通过（net 预设）
- [x] **AC2** 板端：`ls /data/agent/skills` 含三个 Skill（modbus_query + alarm + report）
- [x] **AC3** `cat /data/agent/HEARTBEAT.md` 含 VelaGuard 主动任务说明
- [ ] **AC4** 注入超阈值或离线后，`pending_alarm.txt` 出现；`ai_agent` 运行后 heartbeat 可触发解释（手动或等 30min）— **代码就绪，板测留 §4 可选**
- [x] **AC5** `vela> ask 生成今日运营日报` 按 operations_report Skill 输出（含 vgstats/vgmodbus 证据）— **2026-08-30 13:23 COM3：`daily-20260228.md` 1796B，`END status=ok iters=6 tools=6 elapsed=115s`**
- [x] **AC6** 笔记落盘

## References

- `.trellis/tasks/08-25-agent-capability-rescope/design.md` §3–5
- `VelaGuard_项目手册.md` §14.2
