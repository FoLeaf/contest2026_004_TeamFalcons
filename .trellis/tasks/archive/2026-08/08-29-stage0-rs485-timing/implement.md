# 实施计划：RS485 DIR / TC

> 用户批准本 planning summary 后，再 `task.py start`。

## Phase 0 — 基线

- [x] **0.1–0.3** 根因已由 known-issues §4 + 代码确认（txempty=TXE）

## Phase 1 — 驱动修复（nuttx）

- [x] **1.1** RS485 关 FIFO；`txempty` → **TXE\|TC** 后切 DIR=RX
- [x] **1.2** TC ISR 不切 DIR；shutdown 回 RX
- [x] **1.3** `bash scripts/build.sh emmc` → `.debug/nuttx.hex`

## Phase 2 — 应用去绕过

- [x] **2.1** 去掉 `velaguard_rs485` `usleep(50ms)`（本轮重建）
- [x] **2.2** Modbus：`VG_MODBUS_RTU_GAP_US`
- [x] **2.3** known-issues §4 + research 已记 LA/换收发器教训

## Phase 3 — 板测 AC3（证据 B）

- [x] **3.1** 2026-08-29 22:28：无 usleep 固件，COM6 Hex×6 均为 `61…7A`
- [x] **3.2** `vgrs485 rx` PASS（用户确认）

## Phase 4 — 收尾 AC4

- [x] **4.1** `research/rs485-tc-dir-notes.md`
- [ ] **4.2** commit / archive（待确认）

## 验证命令

```bash
# 构建（按现网预设）
bash scripts/build.sh min   # 或 net / emmc，以含 UART7 RS485 为准

# 板端
vgrs485 tx
vgrs485 tx
vgrs485 tx
# 对端串口工具核对每次 26 字节 abc…xyz
```

## 回滚

恢复 usleep 绕过 + 还原 `txempty` 绑定；known-issues 标回「仅绕过」。
