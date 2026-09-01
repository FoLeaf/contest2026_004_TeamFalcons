# RS485 时序（驱动级 DIR / TC）

> 父任务：`08-29-stage0-foundation`  
> 依据：手册 §5.2、§14；`docs/velaguard-bringup-known-issues.md` §4；推进方案阶段 0。

## Goal

半双工 DIR 在**最后一位停止位之后**再切回接收；去掉应用层 `usleep(50000)`（及 Modbus 同类尾巴延时）作为正确性依赖；驱动修复可整理为 NuttX/openvela **上游 PR**。

## Confirmed Facts（仓库证据）

| 项 | 事实 |
|----|------|
| 设备 | `/dev/rs485` → `/dev/ttyS2` = **UART7**；DIR = **PK1**（`GPIO_UART7_RS485_DIR`） |
| 现况 | `CONFIG_UART7_RS485`：发前拉 DIR；ISR 在 **TC + TCIE、TXEIE 已清** 时把 DIR 拉回接收 |
| 根因 | `uart_tcdrain` 依赖 lower-half `.txempty`；STM32H7 当前把 **`txempty` 接到 `up_txready`（TXE）**，不是 **TC** → drain 在移位寄存器未发完时返回；随后 `close` → `unconfiggpio(DIR)` 松开 PK1 → 下拉切接收 → 帧尾被切 |
| App 绕过 | `velaguard_rs485.c`：`tcdrain` 后 `usleep(50000)`；`modbus_port_openvela.c`：`VG_MODBUS_TX_TAIL_US`（5ms） |
| 手册取向 | 优先驱动级（TC），非应用层延时；公共仓 + 上游 PR |
| 硬件 | DIR 属扩展板；board.h 已定义 PK1；BOUNDARY V11 交叉核对按需 |

## Decisions（已定）

| 决策 | 选择 |
|------|------|
| 验收证据 | **B**：USB-RS485 / 对端收满约定字节作等价证据（不做逻辑分析仪门禁；有探头时可自愿补 A） |
| 主波特率 | 9600（与既有绕过验证一致）；可选抽测更高 |
| 修复目标 | 驱动：`tcdrain`/`txempty` 在 RS485 下等 **TC**；`close` 不提前拆 DIR 导致切向；应用去掉症状级长延时 |

## Requirements

- R1 驱动：RS485 下 drain 返回时最后一字节已物理发完（TC）
- R2 应用：`vgrs485` / Modbus port 不以长 `usleep` 保证帧完整（RTU 帧间隔若保留须单独说明）
- R3 板测（证据 B）：去绕过后连发，对端收满
- R4 笔记 + 可整理上游 PR 说明

## Acceptance Criteria

- [x] **AC1** STM32H7（至少 UART7 RS485）：`.txempty`（或等价路径）在 RS485 下反映 **TC**，使 `tcdrain` 语义正确
- [x] **AC2** 去掉 `velaguard_rs485` 的 `usleep(50000)` 与 Modbus 症状级 DIR 延时后，功能仍完整（仅保留 RTU gap）
- [x] **AC3** 板测 B：2026-08-29 22:28 COM6 Hex×6 均为完整 `61…7A`（无 usleep 固件 + 新收发器）
- [x] **AC4** `research/` 笔记：根因、改动、LA/换收发器教训、上游 PR 要点

## Out of Scope

- 逻辑分析仪作为门禁（自愿补充）
- 帧统计、Modbus 业务、自动波特率
- 非 UART7 的全面矩阵（改动能惠及其他 U[S]ART RS485 更好，但不阻塞 AC）

## References

- `docs/velaguard-bringup-known-issues.md` §4
- `nuttx/arch/arm/src/stm32h7/stm32_serial.c`
- `nuttx/drivers/serial/serial.c`（`uart_tcdrain`）
- `VelaGuard_项目手册.md` §5.2
