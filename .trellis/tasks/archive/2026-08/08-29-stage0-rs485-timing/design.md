# 设计：RS485 驱动级 DIR / TC

## 1. 边界

| 层 | 位置 | 改动 |
|----|------|------|
| 公共仓驱动 | `nuttx/arch/arm/src/stm32h7/stm32_serial.c`（必要时对照 `stm32`/`stm32f7` 同类） | RS485 下 `txempty` 等 TC；shutdown 与 DIR 生命周期 |
| 上层串口 | `nuttx/drivers/serial/serial.c` | 原则上不改接口；若需 ops 语义澄清只加注释/文档 |
| 选手仓应用 | `app/velaguard/velaguard_rs485.c`、`modbus_port_openvela.c` | 去掉症状级 TX 尾巴延时 |
| 笔记 / PR | task `research/` + 可衔接 `08-28-nuttx-upstream-pr-submission` | 复现与上游说明 |

不做：帧统计、LA 门禁、扩展板原理图重画。

## 2. 问题机制

```text
write() → 软件 FIFO
     → TXE：字节进入移位寄存器（txempty 今日误绑在此）
     → TC：移位寄存器发完（含停止位）← DIR 应在此后切回

今日：tcdrain 见「缓冲空 + TXE」就返回
      → close → unconfiggpio(DIR) → 下拉 → DE/RE=RX → 尾巴被切
      → app 用 usleep 掩盖
```

STM32H7 已有 TCIE 路径在 TC 时拉回 DIR；缺口是 **drain/close 与 TC 不同步**。

## 3. 技术方案（推荐）

1. **RS485 专用 `up_txempty`**：若 `rs485_dir_gpio != 0`，则 `txempty` 为「ISR_TC 置位」（或 TXE 且 TC）；非 RS485 保持 `up_txready`，避免拖慢普通 console drain。
2. **shutdown / close**：在 RS485 上先确保 TC（或复用 drain），再 `unconfiggpio`；或 close 时保持 DIR 为接收电平而不拆脚（二选一，实现时以「对端收满」为准选更小 diff）。
3. **应用**：删除 `usleep(50000)` / 评估去掉 `VG_MODBUS_TX_TAIL_US`；若 Modbus 仍需 RTU 3.5 字符间隔，改为按波特率计算的短间隔并注明**不是** DIR 补偿。

备选（不优先）：仅加长 close 延时、或只改 app —— 与手册/上游 PR 目标不符。

## 4. 验收与回滚

- 证据 **B**：去延时后 USB-RS485 收满 26×N。
- 回滚：恢复 `txempty` 绑定 + 临时加回 usleep；记入 known-issues。

## 5. 风险

| 风险 | 应对 |
|------|------|
| 普通 UART 误等 TC | 仅 RS485 实例改 `txempty` |
| DMA TX 路径 | UART7 bring-up 多为 IRQ；若开 TXDMA 需单测 drain |
| 上游可接受性 | 小 diff、对齐 Linux/手册「等发送完成」叙述 |
