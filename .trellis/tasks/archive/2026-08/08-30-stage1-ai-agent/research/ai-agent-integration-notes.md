# ai_agent 正式集成笔记

> task `08-30-stage1-ai-agent` · 2026-08-30

## Verdict

**AC1–AC5 通过** — 编译 + 板端 eMMC 持久化 + LLM/工具链（agent-ops 日报验收覆盖 AC4）

## 集成摘要

| 项 | 值 |
|----|-----|
| 预设 | `velaguard-net`（日常唯一入口） |
| 数据目录 | `CONFIG_EXAMPLES_AI_AGENT_VELA_DATA_DIR="/data/agent"`（eMMC FAT） |
| 入口 | `velaguard_app_main`；`ai_agent` NSH 命令（attach daemon 或独立） |
| 只读查数 | run_shell 白名单：`vgmodbus` `vgstats` `vgcfg` `vgnet` |
| 种子 Skill | `/data/agent/skills/modbus_query.md`（`vg_agent_seed.c`） |
| Autostart | `CONFIG_VG_AGENT_AUTOSTART` → `ai_agent --daemon` |

## 相对探针 preset 差异

- **无** RAMMTD / littlefs（持久化走 eMMC `/data/agent`）
- **保留** velaguard 全套：net failover、eMMC、vgcfg、vgstats、Modbus
- **裁剪** ai_agent 通道：Feishu/Weixin/MQTT/MCP/Node/LVGL

## 板端 AC

- 探针：`scripts/stage1_agent_accept_nsh.txt`
- 运营日报：`scripts/stage1_agent_ops_accept_nsh.txt`（2026-08-30 COM3 通过）

## 文件

| 位置 | 内容 |
|------|------|
| `nuttx/.../velaguard-net/defconfig` | ai_agent + mbedTLS Kconfig |
| `packages/ai_agent/.../tool_shell.c` | VelaGuard NSH 白名单 |
| `app/velaguard/vg_agent_seed.c` | 种子 Skill |
| `app/velaguard/velaguard.c` | autostart daemon |
| `stm32_bringup.c` | agent 子目录 mkdir |

## Next

- `stage1-modbus-discovery`：自动探查点表
- `stage1-lvgl-hmi`：最简 HMI
