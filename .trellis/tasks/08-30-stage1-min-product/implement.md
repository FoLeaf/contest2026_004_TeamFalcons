# 实施计划：阶段 1 剩余闭环

> 父任务不写产品代码。下列清单按顺序在 **已有子任务** 上执行。  
> 下一步实施目标：已 `in_progress` 的 `08-30-stage1-lvgl-hmi`（批准本规划后继续该子任务；**勿** `task.py start` 本父任务）。

## 0. 规划卫生（本步，无固件）

- [x] 父 PRD 对齐 2026-09-01 板测事实
- [x] 父 design / implement 写剩余闭环
- [x] 勾选并归档 `08-25-agent-capability-rescope`（文档已落地）
- [x] 归档 `08-30-stage1-modbus-discovery`（NSH AC 全绿；D3 父勾选已在本 PRD）

## 1. HMI 板端剩余（`08-30-stage1-lvgl-hmi`）

- [ ] C1 目视：裁剪首页、扫描开关默认关
- [ ] C2 探查页 ON → scan → 列表 ≥1（MThings mock @9600）
- [x] C3 屏上 confirm → `vgcfg dump` / `points.json`（代码+NSH 证据；屏上点按待目视）
- [ ] C4 首页出现刚确认的从站（非 mock 24 路）
- [ ] C5 报告页：有日报则摘要，无则 empty 态
- [ ] C6 告警页可见「AI 推测」区块（占位或 pending 文件）
- [x] C7 扩展 `stage1_lvgl_hmi_accept.ps1` 覆盖 C2/C5 的 NSH 对照（屏仍目视）— 2026-09-01 **11/11 PASS**

**验证**

```bash
bash scripts/build.sh velaguard-lvgl
powershell.exe -File scripts/flash.ps1
powershell.exe -File scripts/stage1_lvgl_hmi_accept.ps1
# 目视探查/确认；nsh> vgcfg dump
```

## 2. Agent 与 HMI（Q1=B：尽力，不阻塞出口）

- [x] D1（尽力）HMI 已跑：`ai_agent &` → assert（笔记已记；保持 HMI skip autostart）
- [ ] D2（非出口）恢复 `VG_AGENT_AUTOSTART`

另：`net` 单固件 agent 启动 panic 已修（mallinfo）；`ai_agent --daemon &` + attach 可进 `vela>`。autostart 仍关，待网络稳定后再开。

## 3. CLI 查数板测（`velaguard-net`）

- [ ] E1 `vela> ask` 查从站读数 — **2026-09-01 进展**：
  - 根因：`ai_agent` 启动在 `mallinfo()` 触发 NuttX heap walk assert → 已 patch `packages/ai_agent/src/agent_main.c` 跳过 NuttX boot mallinfo
  - 验收：`stage1_agent_accept.ps1` **6/8**（`vela>` PASS；`net_test`/真实 `ask` 待网络）
  - eMMC：`provision-llm-from-secrets.ps1` OK → `config.json` 含 `token-plan-cn.xiaomimimo.com` / `mimo-v2.5`
  - 阻塞：板端 `ifconfig` eth0 `0.0.0.0`，`vgnet status` wifi `assoc=0` → TLS/DNS 失败（环境/接线）
## 4. 可选：事件主动 AC4

- [ ] F1 注入超阈值或离线 → `pending_alarm.txt` → heartbeat 解释  
  跳过则父 AC「事件主动」保持可选，§14.2 告警比例不填数字。

## 5. 父出口

- [ ] 对照手册 §14.1 逐条
- [ ] 日报 §14.2 沿用 2026-08-30 证据；写操作 = 0
- [ ] `task.py archive 08-30-stage1-min-product`（全部子任务归档后）

## Rollback

- 烧 `velaguard-net` 回到无屏闭环
- HMI confirm 只写 inactive slot；`vgcfg damage` 可回 factory
