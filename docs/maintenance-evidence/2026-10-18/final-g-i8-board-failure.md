# G-I8 板端失稳现场与恢复记录

记录时间：2026-10-18 22:14 至 23:10（Asia/Shanghai）

本记录只说明现场观察和可复核的原始数据，不把复位后的恢复当作故障根因已经解决。
I8 离线滑窗工况没有形成完整的 16 个连续失败采样，不能判通过。QA-04 继续保持
未关闭。

## 对象

- 固件：`NuttX 0.0.0 2373ab4a45d Oct 18 2026 21:50:34`
- 产品源码：`c20177f21e01bd70a4c96d12dc11095ebfcd08fd`
- 从站：COM7，9600 8N1，0 ms，slave-only
- 复位原因：人工 OpenOCD `reset run`，记录于
  [reset-log.txt](reset-log.txt)

## 触发前的正常运行

从站在 22:05:17 启动后先运行 `normal` 基线。22:05:17 至 22:14:32 的
STATUS 累计请求和应答持续增长，`crc_scan=0`。22:07 至 22:14 的板端只读记录
也显示已确认表 14 点在线，因此本次 I8 不是从板上电后的首个工况。

原始从站日志：
[final-g-recovery-slave.log](final-g-recovery-slave.log)，SHA256
`e5b95db1c0f6b8a568326a1cbfadb027eb806716325cafb3997f312d59ab5bb8`。

## I8 触发与失稳

22:14:32 写入并生效：

```text
line="drop 7 0 32" effect=drop slave=7 reg=0 n=32
```

从站 STATUS 只记录到 4 次 drop。请求/应答累计在 22:14:47 左右停在
`requests=3758 replies=3754 dropped=4`，此后直到 22:23:47 再无新请求。原日志
没有开启逐请求记录，因此不能从这里补出缺失的 drop 请求。

板端随后出现：

- `vgpoint get door`、`vgruntime dump`、`uptime`、`ps` 均返回零长度。
- 启动补采只看到后台网络和 MQTT 凭证缺失提示，没有新的 NSH 提示符。
- 串口日志中没有 HardFault、panic 或 watchdog 文本。
- [最终 I8 板端零响应日志](final-g-i8-board-check-221559.log)

## 调试器观察

在未复位目标的情况下连接 OpenOCD，目标停在异常处理上下文，关键现场为：

```text
pc 0x90001082 <exception_common+122>
lr 0xffffffe9
sp/msp 0x24048030
```

连续采样中 `g_system_ticks` 保持 `0x0001fe40` 不变，而 SysTick 当前值持续变化。
NVIC 采样显示 IRQ 39 活跃，向量表中该入口对应 `up_interrupt`，串口寄存器现场为
`CR1=0x2000002d`、`ISR=0x0ce010f8`。这些数据说明内核 tick 已停止推进，USART3
中断现场存在持续请求和错误状态，但仅凭这些数据不能证明 USART3 是唯一根因，也不能
证明 `drop 7 0 32` 必定导致该状态。

原始记录：

- [OpenOCD 无复位连接](final-g-i8-openocd-noreset-20261018-2220.log)
- [GDB 异常上下文快照](final-g-i8-gdb-snapshot-20261018-2222.log)
- [tick 采样](final-g-i8-tick-sampling-20261018-2224.log)
- [NVIC 和 IRQ 采样](final-g-i8-irq-sampling-20261018-2226.log)
- [USART3 寄存器采样](final-g-i8-usart3-state-20261018-2228.log)
- [USART3 handler 采样](final-g-i8-usart3-handler-20261018-2230.log)
- [SysTick 与 USART 相关采样](final-g-i8-systick-usart2-20261018-2232.log)

## 恢复

22:24:35 由人工执行 OpenOCD `reset run`。复位启动日志显示 eMMC、HMI、采集线程和
Agent 均重新启动：

- [复位启动日志](final-g-i8-recovery-boot-20261018-2236.log)
- [复位后只读复查](final-g-i8-postreset-readonly-20261018-2238.log)

22:27:38 重新启动同一 COM7 从站后，22:28:12 和 22:31:19 的只读记录均显示
`vgpoint get` 14 点 `ok=1`，`vgruntime dump` 14 点 `online=1 kind=none`。

## QA-04 第三次复现

22:53:45 的第三次复现中，`vgagent status`、`ls /data/velaguard/pending_alarm.txt`
和报告目录列举均返回零长度。原始记录为
[第三次零响应日志](qa04-third-zero-response-20261018-225345.log)，SHA256 为
`1125939a5b10709120adca5c281ec9a0b1ff9766e994ff69d73eeb7954b29fd3`。

同一窗口 COM7 从站停在 `requests=10347 replies=10326`，`crc_scan=0`。请求和应答
相差 21 次，但该日志没有逐帧记录，不能据此判断缺失帧发生在主机、RS485 还是板端接收路径。

### 未单步的自然现场

使用 WCH OpenOCD 的无复位配置连接后，在任何调试器单步之前记录了以下现场：

- OpenOCD 多次采样停在 `Handler External Interrupt(39)`，一次为
  `PC=0x90000de8`、`xPSR=0x21000037`、`MSP=0x24048030`；GDB 的另一次快照停在
  `modifyreg32` 的 `PC=0x90000c40`，同样处于外部中断上下文。
- `g_system_ticks` 在至少 5 秒内保持 `0x00028707`，而 `SYST_CSR=0x00010007`，
  说明 SysTick 已使能但内核 tick 没有继续推进。
- USART3 寄存器为 `CR1=0x2000002d`、`ISR=0x0ce010f8`。
- 目标在 USART3 中断入口和 PendSV 之间反复切换，但没有恢复调度推进。

原始记录：

- [无复位 OpenOCD 连接](qa04-third-openocd-noreset-20261018-2257.log)
- [GDB 现场快照](qa04-third-gdb-snapshot-20261018-2258.log)
- [栈和中断上下文采样](qa04-third-openocd-stack-20261018-2302.log)

### IRQ 向量地址更正

USART3 使用 NuttX IRQ 55，即 `STM32_IRQ_FIRST(16) + 39`。`g_irqvector` 的基址为
`0x24003e40`，每个入口 8 字节，因此 USART3 入口为
`0x24003e40 + 55 * 8 = 0x24003ff8`，读到的处理函数是 `0x90000945`
（`up_interrupt+1`）。

此前文档把 `0x24003f78` 当作 USART3 入口，该地址实际是 `g_irqvector+312`，
对应 IRQ 39；`0x24003ff0` 实际是 `g_irqvector+432`，对应 IRQ 54。这两个地址
都不是 USART3 入口。原始采样日志里的标签保留不改，本页以此处更正为准。

### 调试器诱发异常，不能作为根因证据

在尝试从异常入口执行侵入式单步跟踪时，目标进入了 HardFault。最终现场为
`PC=0x00000000`、`MSP=0x24048010`、`CFSR=0x00000100`、
`HFSR=0x40000000`。这次 HardFault 发生在调试器单步之后，不能作为自然故障的
根因状态，也不能用它的寄存器值推断原始零响应原因。

记录见
[调试器诱发异常与恢复](qa04-third-induced-hardfault-reset-20261018-2310.log)。

### 人工复位与恢复

2026-10-18T23:03:23+08:00 执行一次明确的 OpenOCD `reset run`，原因记录为
恢复调试器诱发的 HardFault，不把这次复位当作 QA-04 根因处理。23:03:27 关闭
OpenOCD 会话，未留下调试器连接。

23:04 的 COM3 复验显示板端重新启动，`uname -a` 仍为
`NuttX 0.0.0 2373ab4a45d Oct 18 2026 21:50:34 arm stm32h750b-dk`。
候选表仍为 `温度验收`，已确认表仍为 `温度`，`vgcfg dump` 仍为
`seq=6 schema=1 committed=1 name=vgpoint`，`pending_alarm.txt` 不存在。没有发送
`vgpoint apply --confirm`。原始记录见
[复位后板端只读复验](qa04-recovery-board-verify-20261018-2304.log)。

## 结论

- I8 没有完成，状态记为未验证，不判通过。
- 本次观察到 tick 停止和 USART3 中断现场，但没有足够证据把故障归因于 drop 控制、
  串口接收路径或某一个驱动缺陷。
- QA-04 仍为 P0 未定位事项，第三次复现后现场才恢复；调试器单步诱发的 HardFault 已单独
  标注，不能混入自然故障证据。
- 下一次复现时必须先保存现场，再人工复位；不能以恢复后的正常运行关闭本项。
