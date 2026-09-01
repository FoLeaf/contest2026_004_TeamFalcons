# stage1-agent-ops 笔记

> 2026-08-30 · AC1–AC3/AC5–AC6 通过；AC4 可选

## 交付摘要

| 项 | 实现 |
|----|------|
| Skills | `alarm_interpretation.md`、`operations_report.md`（`vg_agent_seed.c` 首次写入 eMMC） |
| HEARTBEAT | `/data/agent/HEARTBEAT.md` — pending 告警 + 缺失日报 |
| 事件主动 | `vg_agent_alarm.c` 轮询从站 1：超阈值 / 离线（vgstats）→ `pending_alarm.txt` |
| 定时主动 | 依赖 ai_agent heartbeat（30min）+ HEARTBEAT 日报任务；或 `ask` 交互 |
| 后台 Agent | `CONFIG_VG_AGENT_AUTOSTART` → `ai_agent --daemon`（cron/heartbeat 常驻） |
| 交互附着 | NSH `ai_agent` 附着 daemon CLI，不双实例 |
| 镜像 | flash ≈ **550 KB**（+3 KB vs ai-agent only） |

## 板测证据（2026-08-30 13:19–13:26 COM3）

- 固件 build tag：`2026-08-30 13:19:33`
- 流程：`nsh> ai_agent` → `vela> ask …` → `quit`；daemon 保持运行
- 工具链：`read_file` → `get_current_time` → `vgcfg` / `vgstats` / `vgmodbus` → `write_file`
- 产出：`/data/velaguard/reports/daily-20260228.md`（1796 bytes）
- 结束：`END status=ok iters=6 tools=6 elapsed=115s`
- 验证：`ls` + `cat` 全路径文件名（NSH 不支持 `daily-*.md` glob）

## 联调修复（packages/ai_agent + app/velaguard）

| 问题 | 修复 |
|------|------|
| TLS 时钟跳变致 LLM watchdog 误超时 | `agent_loop.c` 用 `CLOCK_MONOTONIC`；启动 `ensure_wall_clock_for_tls()` |
| autostart 与 NSH stdin 冲突 / 双实例 | `--daemon` + `nsh_commands_set_detach_quit` |
| `/data` symlink 下 read/write 路径校验失败 | `tool_files.c` 对 `realpath(AGENT_DATA_DIR)` 校验 |
| `list_dir` 捷径提前结束多轮工具 | 从 local shortcut 移除 `list_dir` |
| MiMo API `content: null` 400 | `llm_proxy.c` 清洗消息；Kimi 才回显 reasoning |
| `run_shell` 非零退出丢弃输出 | `tool_shell.c` 始终返回 JSON |
| 多工具轮次超 60s | `AGENT_LLM_TIMEOUT_SEC` → 120 |

## 板测脚本

- `scripts/stage1_agent_ops_accept_nsh.txt`
- `scripts/stage1_agent_ops_accept.ps1`（COM3；无 `MIMO_API_KEY` 时读 eMMC key）

## 已知限制

- RTC 未校时 → 日报日期显示 **2026-02-28**
- 无从站时 `vgstats`/`vgmodbus` popen exit=-1；Agent 仍写报告（零值/部分数据）
- NSH `cat daily-*.md` → errno 22；用完整文件名
- AC4 告警解读：§4 脚本可手动注入 `pending_alarm.txt`；heartbeat 30min 或 `ask` 触发

## Next

- 可选：板测 AC4（`last_alarm.md`）
- `stage1-modbus-discovery`：自动扫描 + 点表
- `stage1-lvgl-hmi`：首页 + 告警 + 报告预览
