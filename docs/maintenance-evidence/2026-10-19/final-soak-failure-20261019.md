# 2026-10-19 正式长测失败归档

记录时间：2026-10-19 15:25（Asia/Shanghai）

## 结论

运行 `final-soak-20261019T124500` 以 `FAIL` 结束，不能判为通过。前 7 轮采集、点表一致性和存储写入检查均通过。第 8 次 `vgstoretest 50` 在 14:42:32 发出后超过 240 秒没有返回，长测脚本在 14:46:32 写入：

```text
FAIL run=final-soak-20261019T124500 status=FAIL ...
reason=Command timeout after 240s: vgstoretest 50
RUN_END run=final-soak-20261019T124500 status=FAIL ...
```

该运行只覆盖 7,000 秒左右，不能作为任何版本的 24 小时验收。

## 运行身份与边界

| 项目 | 值 |
|---|---|
| 固件 | `NuttX 0.0.0 2373ab4a45d Oct 18 2026 23:48:40 arm stm32h750b-dk` |
| 长测开始 | 2026-10-19 12:43:21 +08:00 |
| 预期结束 | 2026-10-20 12:43:22 +08:00 |
| COM3 | 板端 NSH |
| COM7 | 模拟 Modbus 从站，Windows PID 21776 |
| 周期 | 900 秒 |
| 每轮存储压力 | `vgstoretest 50` |
| 在线下限 | 14 点 |
| 单命令上限 | 240 秒 |
| Windows 长测 PID | 3908，已退出 |

长测启动元数据记录的源码基线为 `65e99ad`，板上运行的是 23:48:40 构建。它是旧镜像，不是 2026-10-19 14:01:50 的新候选镜像。新候选镜像的构建身份和哈希见 [最终候选构建来源](final-build-provenance-20261019T1402.md)。

## 冻结前结果

第 1 至第 7 轮均有以下结果：

- `online=14/14`
- 14 行已确认点表每轮仍为 `committed n=14`
- 每次 `vgstoretest 50` 返回 `OK rounds=50 bytes=25600`
- `uptime_s` 单调增加，没有观察到自然复位
- 堆 `heap_maxused=393448`，各轮使用量在 305504 至 305744 字节之间

| 轮次 | 命令返回耗时 | `elapsed_s` | `uptime_s` |
|---|---:|---:|---:|
| 1 | 121,883 ms | 124 | 700 |
| 2 | 117,216 ms | 1,143 | 1,719 |
| 3 | 118,579 ms | 2,163 | 2,739 |
| 4 | 118,074 ms | 3,183 | 3,759 |
| 5 | 122,138 ms | 4,207 | 4,783 |
| 6 | 118,808 ms | 5,228 | 5,804 |
| 7 | 120,347 ms | 6,250 | 6,826 |

第 7 轮结束时，14 个点位仍在线，温度等模拟值为 24°C。日志最后一条完整的板端运行输出是普通网络重连消息；失败的是下一次存储压力命令，不应把后续的网络消息当作板端仍然活跃的证明。

## 冻结后的独立观察

长测脚本超时后，单独打开 COM3 只读监听 45 秒，时间范围为 14:50:36 至 14:51:21，收到 0 字节。同期 COM7 从站进程仍存活，累计计数冻结在：

```text
requests=46623 replies=46623
```

从站没有新增请求，说明板端采集路径也已停止，而不只是 NSH 显示停止。

未对板端执行复位、烧录、点表写入或存储写命令。点表保持 14 行已确认状态，温度模拟输入保持 24°C。用户当时暂时无法目视屏幕，因此本记录不包含屏幕名称或刷新结论。

## QA-04 的只读现场

长测冻结后的同一运行中还取得了多次 OpenOCD/GDB halt 快照。每次读取后执行 `monitor resume; detach`，没有单步、复位、写寄存器或写内存。证据表只保留寄存器、队列计数和异常状态，不复制 UART 缓冲原文，因为原始缓冲中含 WiFi 凭据。

| 观察项 | 结果 |
|---|---|
| `g_system_ticks` | `0x000aca9b`，多次独立快照保持不变 |
| 按 `CONFIG_USEC_PER_TICK=10000` 折算 | 708,763 ticks，约 7,087.63 秒 |
| `g_system_tick_lock` | `0x000aca9b`，seqcount 为偶数，未显示持锁未释放 |
| `SYST_CSR` | `0x00010007`，SysTick 硬件保持启用 |
| `SYST_CVR` | 每次快照均变化，硬件计数仍在运行 |
| `CFSR`、`HFSR`、`MMFAR`、`BFAR` | 全为 0，不是自然 HardFault |
| 常见 `ICSR` | `0x0043680e`，active=PendSV 14，pending=USART2 54 |
| 另外取得的状态 | active=USART2 54、pending=USART3 55 |
| CPU 位置 | PendSV、USART2、USART3 与 `exception_common` 间流动 |
| `PRIMASK` | 0 |
| `BASEPRI` | 有时为 0，有时为 `0x80` |

USART2 是 ESP-01S，USART3 是 console。两者的 `CR1` 均为 `0x2000002d`，RXNE 中断启用；两者的 `CR3` 均为 `0x06000000`，错误中断 EIE 未启用。独立重读后，软件环形队列如下：

| UART | xmit head/tail | recv head/tail | size | 软件队列 |
|---|---|---|---:|---|
| USART2 | 245/245 | 130/130 | 252 | 空 |
| USART3 | 230/230 | 168/168 | 252 | 空 |

因此当前现场不能再用接收队列满解释冻结。硬件 RXNE 或 ORE 条件仍在出现，而中断返回路径没有让普通任务和 SysTick 持续推进；唯一根因仍未确认，QA-04 不能关闭。

TCB 现场也处于异常路径而非普通任务运行：

- `g_readytorun` 头尾均指向 `0x24003d5c`
- 对象为 `g_idletcb`，名称 `CPU IDLE`
- task state 为 3，即 RUNNING
- `g_running_tasks` 同样指向该 TCB

该结果与 CPU 困在异常或 IRQ 路径一致，但不能单独证明是哪个中断源造成饥饿。

### 15:12 至 15:14 连续采样

之后又进行了两组连续只读采样，共 36 个 halt 快照。两次采样都只执行 `monitor halt`、读取寄存器、`monitor resume` 和 `detach`，没有单步、复位、写内存或写寄存器。所有 36 个快照的 `g_system_tick_lock` 仍为 `0x000aca9b`，而 `SYST_CVR` 持续变化。

NVIC enable 位图恒为：

```text
0x00000100 0x200200c0 0x81040000 0x00000001
```

据此启用的是外部 IRQ 8、38、39、49、61、82、88、95、96。pending 位图有两种状态：

- 35 个快照为 `0x00000000 0x200000c0 0x12000000 0x00000000`，即 IRQ 38、39、61、89、92 pending。
- 1 个快照为 `0x00000000 0x20000080 0x12000000 0x00000000`，即 IRQ 39、61、89、92 pending，同一时点 IRQ 38 active。

因此 LTDC error IRQ 89 与 QSPI IRQ 92 虽然保留 pending 状态，但 NVIC 未启用，不能进入异常入口。当前真正同时处于 enabled 与 pending 状态的中断源是 Ethernet IRQ 61 和 USART2/3 IRQ 38/39。该结果把怀疑范围从单一 UART 扩展到多源中断与共享异常返回路径，但仍不能只凭 pending 位认定哪个中断产生饥饿。

ICSR 在 35 个快照中为 `0x0043680e`，即 active=PendSV 14、pending=USART2 54、`ISRPENDING=1`。在 1 个快照中为 `0x00437836`，即 active=USART2 54、pending=USART3 55。采样期间 CPU 位于 `exception_common`、`exception_direct`、`arm_doirq` 和这些异常入口之间。QSPI 在构建配置中未启用驱动，LTDC error 也未被驱动注册为独立处理器，两者不能按 enabled 中断源继续分析。

## 证据文件

原始长测和只读采样位于被忽略的 `.debug/`，以下哈希用于回指原始文件；敏感 UART 原文没有放入 `docs/`。

| 文件 | 用途 | SHA256 |
|---|---|---|
| `.debug/final-soak-20261019T124500.log` | 完整长测日志，末行为 `RUN_END status=FAIL` | `e181dc23cb5864b70acd30045000da7313190faaa7d2a8ac6e016c0fa7fb3fc6` |
| `.debug/final-soak-fail-observe-20261019T1450.log` | 45 秒无字节观察 | `285a9c5f151f7abe9bf55081f207465a790e3842099e6859ff892779d3fb121b` |
| `.debug/qa04-readonly-snapshot-20261019T1515.gdb` | 只读采样命令 | `6391c8aa3a35bc89bc882d716f05cbe460bcb2d11f388c92c41715e584974248` |
| `.debug/qa04-readonly-snapshot-20261019T1515-A.log` | 新候选 ELF 符号采样 | `00273ec2cfbf44bdbf5ad4c86c57ae8ee2f42239e89b911dcb5af490c1d2c12c` |
| `.debug/qa04-readonly-snapshot-20261019T1515-B-oldelf.log` | 旧 ELF 符号交叉采样 | `589739558baab5d7c92768cb124c3f6d9a3b8082cd155ab6c521a24b2dad004f` |
| `.debug/qa04-readonly-snapshot-20261019T1522-C-oldelf.log` | 约 3 秒后的独立快照 | `3ef24fdb8ce59266a129f00b0efbe1f232ff5b114409daffee8574d7a4694384` |
| `.debug/qa04-readonly-samples-20261019T1535.log` | 24 次连续只读采样 | `b875118e1ca5cf1270a56014d61a9a360c4c09799f4b5bacfbdf01afa13f9802` |
| `.debug/qa04-readonly-samples-20261019T1540.log` | 12 次连续只读采样 | `9e06fd18b98d871d37dc8937d20a670aa4302bed495f52adb10e74266b1c77e0` |
| `.debug/qa04-readonly-samples-20261019T1540.gdb` | 第二组采样命令 | `af5f4c9b5e1cf58a40b06e43fb092a05f54f09b4f248c7d6f5a95fce3fc760b7` |

B 与 C 文件所用 `board-c20177f-before-rebuild/nuttx.elf` 是 21:50 构建，不是板上确切的 23:48 ELF。它们只能作为 RAM 地址、结构布局和关键文本映射的旁证，不能把其中所有产品函数地址都当成板上精确源码位置。A 使用 14:01 候选 ELF，也不是板上 23:48 ELF。

## 后续关闭条件

本次失败已经形成可追溯现场。后续必须先确认已启用 IRQ 或异常返回路径的确定性根因，再做源码修复。修复后需要：

1. 完成 clean build 并核对产物哈希。
2. 同时验证 QSPI 主镜像和内部 boot stub 下载成功。
3. 用新镜像从冷启动开始独立完成完整连续 24 小时长测。
4. 只有同一运行写出 `PASS` 和 `RUN_END status=PASS` 才能关闭长测项。

旧镜像这 7 轮通过不能转移给新镜像，也不能据此关闭 QA-01 或 QA-04。
