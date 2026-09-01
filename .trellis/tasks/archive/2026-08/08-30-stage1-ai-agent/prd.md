# ai_agent 正式集成（eMMC 持久化）

> 父任务：`08-30-stage1-min-product`  
> **依赖**：`08-30-stage1-data-layout`（`/data/agent` 已就绪）  
> **探针**：`08-29-ai-agent-feasibility-probe` AC1–4 已通过

## Goal

在 **`velaguard-net`** 日常预设上启用 ai_agent，数据目录 **`/data/agent`**（eMMC FAT），提供只读 Modbus/统计查询能力，支持 CLI 自然语言查实时数据。

## 刻意排除（留给后续子任务）

- Skill `alarm_interpretation.md` / `operations_report.md` → `stage1-agent-ops`
- 事件主动 / 定时日报 → `stage1-agent-ops`
- LVGL → `stage1-lvgl-hmi`
- 自定义 C 工具深层沙箱单元测试

## Requirements

- R1 `velaguard-net` 合并探针最小 Kconfig（mbedTLS + ai_agent，无 RAMMTD）
- R2 `CONFIG_EXAMPLES_AI_AGENT_VELA_DATA_DIR="/data/agent"`
- R3 run_shell 白名单含 `vgmodbus` / `vgstats` / `vgcfg`（只读查数）
- R4 首次启动种子 Skill `modbus_query.md`
- R5 `bash scripts/build.sh net` 编译链接通过

## Acceptance Criteria

- [x] **AC1** net 预设编译链接通过
- [x] **AC2** 板端：`ai_agent` → `vela>`；`/data/agent/config` 可写（daemon attach 流程）
- [x] **AC3** 冷复位后 `/data/agent` 内文件仍在（eMMC 持久化）
- [x] **AC4** `net_test` 通；`set_llm` + `ask` 查从站 1 温湿度（NL 或 skill 引导）— **并入 agent-ops 日报链路验证**
- [x] **AC5** 笔记落盘

## References

- `.trellis/tasks/archive/2026-08/08-29-ai-agent-feasibility-probe/research/probe-report.md`
- `VelaGuard_推进方案.md` §6.1 ai_agent 集成
