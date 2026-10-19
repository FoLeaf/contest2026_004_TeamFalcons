# QA-04 冻结诊断第一轮（2026-10-19 19:32–22:40）

本轮目标：用最小诊断改动区分冻结时中断是否仍在返回、SysTick 是否进入、普通线程是否继续运行。

**结论：没有取得冻结现场。诊断目标未完成。** 诊断固件已构建、烧录并运行，计数器在正常状态下工作正确；第二次长测运行约 20 分钟后被脚本判为"板端重启"而退出。已有计数未回零，但不足以确定启动序列的来源；本轮不认定自主复位，也未取得原冻结复现证据。

## 一、固件与来源

| 项目 | 值 |
|---|---|
| 镜像内标识 | `1b5e71d2f87 Oct 19 2026 19:38:59 arm stm32h750b-dk` |
| 竞赛仓 | `codex/maintenance-closeout`，`e1a459bfa5b5d3c7c526dd72c2a5f67dea28ea01` |
| NuttX | `velaguard/eth-mii-stm32h750b-dk`，`1b5e71d2f8732e4e3338fc4252167a0fe4adb9b1` |
| apps | `velaguard/netinit-esp8266`，`0ea954b79740` |
| ai_agent | `velaguard/llm-tls-send-retry`，`7dc0a8816292` |
| MQTT-C | `velaguard/mqtt-pal-hook`，`4dbb7f321cee` |
| 构建 | `bash scripts/build.sh --clean` 全量，之后增量重建一次使版本串包含诊断提交；0 warning |
| 配置 | `CONFIG_DEBUG_FREEZE_DIAG=y`（nuttx 新增，默认关闭），其余沿用 `velaguard-lvgl` |

产物摘要（`.debug/` 不入版本控制，这里只记录摘要）：

| 文件 | SHA256 |
|---|---|
| `nuttx.elf` | `037a45e4577fe24bb180bc234e4324c1b582f5b63c43529ada7869b8c3e63bac` |
| `nuttx.bin` | `2b1ec34fc5e56c2e17f2205ad855b4855de095dc0126d7c4ff54181545d67422` |
| `nuttx.hex` | `f8381033515bb78652306674c35ea50acaa8ffa3fea32b109a36de2eb464c3d7` |
| `qspi_bootstub.hex` | `877aa3cf6072d66582b33eac555ded7cd5603bacd5dc794915fbcba425f6119e` |

匹配的 ELF 保留在 `.vg-maintenance-closeout/.debug/nuttx.elf`（SHA256 与上表一致），未删除。关键符号：`exception_common=0x900010ad`、`exception_direct=0x90000e41`、`arm_doirq=0x90000e79`、`stm32_timerisr=0x9000b8e5`、`up_interrupt=0x90000945`、`g_arm_freeze_diag=0x24003cd0`、`g_system_ticks=0x24007394`。

## 二、诊断改动

| 文件 | 内容 |
|---|---|
| `arch/arm/src/common/arm_freeze_diag.h/.c` | `struct arm_freeze_diag_s`，20 个 `uint32_t`，共 80 字节，位于 `.bss` |
| `arch/arm/src/armv7-m/Kconfig` | 新增 `CONFIG_DEBUG_FREEZE_DIAG`，默认 n |
| `arch/arm/src/stm32h7/stm32_timerisr.c` | SysTick 入口/出口计数，出口时记录 `g_system_ticks` |
| `arch/arm/src/armv7-m/arm_doirq.c` | PendSV 入口/出口计数；入口时记录 `BASEPRI`、`PRIMASK`、`IPSR`、栈帧 `xPSR`、`xcp.regs`、`xcp.saved_regs`；`up_schedule_sigaction` 调用次数；空 context 次数 |
| `arch/arm/src/stm32h7/stm32_serial.c` | USART2 入口/出口计数，`uart_recvchars()` 前后计数，入口时的 `ISR`、`priv->ie`、RX FIFO 电平 |
| `arch/arm/src/common/Make.defs` | 条件编译上述源文件 |
| `scripts/configs/velaguard-lvgl.defconfig`（竞赛仓） | 打开该选项 |

普通线程进度沿用已有的 `g_live_cycle`（HMI 采集线程每完成一次轮询加一），未新增计数。中断内只有一次加载/加一/写回，不打印、不等待、不分配内存、不加锁；未改动任何中断优先级或屏蔽规则。

## 三、运行结果

### 3.1 第一次启动（19:42:36，`qa04-freezediag-20261019T1945`）

启动即退出：

```text
FAIL run=qa04-freezediag-20261019T1945 status=FAIL time=2026-10-19T19:42:36.6199201+08:00
reason=Board boot banner seen in before ''; the board restarted during the soak.
RUN_END run=qa04-freezediag-20261019T1945 status=FAIL time=2026-10-19T19:42:36.6287675+08:00
```

原因是长测脚本打开 COM3 时，串口里还留着 OpenOCD `reset run` 之后板端输出的启动日志，`Assert-NoFirmwareFault` 把它当成运行中的重启。**这是脚本的判定条件与"刚烧录/刚复位"状态冲突，不是板端缺陷。** 排空串口缓冲后重启长测。

### 3.2 第二次启动（19:52:31，`qa04-freezediag-20261019T1953`）

| 轮次 | 时间 | `vgstoretest 50` | `uptime_s` | 在线 | 堆 used |
|---|---|---|---|---|---|
| 1 | 19:54:32 | OK，119.4 s | 803 | 14/14 | 230,896 |
| 2 | 20:11:31 | OK，117.1 s | 1,822 | 14/14 | 231,136 |

20:12:15 脚本再次报"板端重启"并退出：

```text
FAIL run=qa04-freezediag-20261019T1953 status=FAIL time=2026-10-19T20:12:15.3783737+08:00
reason=Board boot banner seen in interval wait; the board restarted during the soak.
```

### 3.3 这条"重启"判定不成立

同一份日志里可以逐项对照：

| 证据 | 第 1 轮（19:54） | 第 2 轮（20:11） | 含义 |
|---|---|---|---|
| `vgruntime: uptime_s=` | 803 | 1,822 | 差 1,019 s，与两轮实际间隔一致，**没有回零** |
| `bus boot_total` | 5,550 | 12,614 | 单调增加，**没有回零** |
| 已确认点表 | `committed n=14` | `committed n=14` | 未变 |
| `llm: credentials=` | ready | ready | 未变 |
| 第 1 轮总线统计 | `boot_crc=0 boot_timeout=0 boot_echo=0 boot_other=0` | 同 | 链路正常 |

启动横幅是 `vgnet: manager started` 之后才出现在日志里的，而 `vgtime: restore 2026-10-19 20:11:02` 这一行本身也证明：它执行的是**开机恢复**路径。正常状态下的两次调试器采样早于 20:11 的启动序列，不能据此证明此后一直连续运行。

日志中启动序列的来源及是否存在外部复位，**现有记录无法确定**。缓冲滞留、重新打印等解释均未验证。本轮只确认"计数器与 uptime 没有回零"，因此**不认定板端自主复位**，也**不认定为原冻结复现**。本轮操作记录中没有人工复位：两次烧录之间只执行过一次 OpenOCD `reset run`（19:40，用于连接），20:11 前后没有任何调试器或烧录动作。

### 3.4 正常状态下的计数器样本

19:42 连接后立即做了两次 halt 采样（间隔约 3 秒，均为只读，采样后 resume）：

| 字段 | 样本 1 | 样本 2 | 差值 |
|---|---|---|---|
| `systick_entries` | 7,469 | 7,777 | +308 |
| `systick_exits` | 7,469 | 7,777 | +308 |
| `systick_last_tick` | 7,469 | 7,777 | +308 |
| `pendsv_entries` | 14,685 | 15,212 | +527 |
| `pendsv_exits` | 14,685 | 15,212 | +527 |
| `pendsv_sigaction` | 0 | 0 | 0 |
| `pendsv_null_regs` | 0 | 0 | 0 |
| `pendsv_last_basepri` / `primask` / `ipsr` | 0 / 0 / 14 | 0 / 0 / 14 | — |
| `pendsv_last_xpsr` | `0x81000200` | `0x81000200` | — |
| `pendsv_last_regs` | `0x24048020` | `0x24048020` | — |
| `pendsv_last_saved` | 0 | 0 | — |
| `usart2_entries` | 7,803 | 7,920 | +117 |
| `usart2_exits` | 7,803 | 7,920 | +117 |
| `usart2_recv_before` | 7,609 | 7,712 | +103 |
| `usart2_recv_after` | 7,609 | 7,712 | +103 |
| `usart2_last_isr` | `0x000010f0` | `0x00001090` | — |
| `usart2_last_ie` | 32 | 160 | — |
| `usart2_last_rxlevel` | 0 | 0 | — |
| `g_live_cycle` | 35 | 37 | +2 |
| `g_system_ticks` | 7,469 | 7,777 | +308 |

两次采样间隔 3.14 s，对应 308 个 10 ms tick，与 `g_system_ticks` 的增量一致。

`0x81000200` 解出 IPSR 域为 0、Thumb 位为 1，即被 PendSV 打断的是线程模式；`0x24048020` 落在主栈区域。`usart2_last_isr` 的 `0x000010f0` 对应 `IDLE|RXNE|TXE|EOBF` 等位，RX FIFO 电平为 0。

需要说明的边界：

- 入口与出口计数相等，只说明采样时刻没有未完成的调用，**不能**证明该路径不会卡死。冻结时若入口远大于出口，也只能说明"采样时有一次调用没有返回"。
- `recv_before` 与 `recv_after` 相等同样只反映采样时刻。`usart2_entries` 与 `recv_before` 之差（本样本 194）来自进入中断但没有走到接收分支的次数，不能据此判断循环空转。
- `usart2_last_ie` 从 32 变到 160，是最后一次中断时 `priv->ie` 的快照，随收发状态变化，不构成异常证据。
- **SysTick 是否在冻结时仍被服务，本轮没有得到任何冻结样本。** 此前"瞬时采样没看到 SysTick"仍是待验证判断，不能当作已证明。

## 四、采集脚本修正

`qa04-freeze-diag-halt.gdb` 原有两处问题，已在离线核对后修正（未连接开发板补测）：

1. **无类型符号**：本轮镜像是 `CONFIG_DEBUG_SYMBOLS=n` 的发布构建，`print g_arm_freeze_diag` 与 `print g_system_ticks` 都会报 `has unknown type`。现已改为按地址逐字读取，不再依赖类型信息。
2. **地址与布局**：用真实头文件编译探针得到 `struct arm_freeze_diag_s` 的布局（`sizeof=80`，20 个 4 字节字段），脚本里 20 个字段地址与探针逐项一致；`g_system_ticks`、`g_system_tick_lock`、`g_live_cycle`、`g_live_n`、`g_acq_started` 的直读地址与 ELF 符号表一致。符号 `g_arm_freeze_diag` 紧邻的下一个符号从 `0x24003d20` 开始，结构体范围 `0x24003cd0..0x24003d1f` 没有越界。
3. PC/LR 仍用 `info registers` 读取，避免上一轮 `*(unsigned int*)$pc` 读成"地址处数据"的错误。

修正后的脚本**尚未在真实冻结现场运行过**，因此只能说明它按当前 ELF 的布局是正确的，不能说明它在冻结时一定给出有效结果。

## 五、未完成与待验证

- **本轮没有取得冻结现场计数。** QA-04 的根因仍未定位，诊断目标未完成。
- 冻结没有复现。第二次长测实际运行约 20 分钟（19:52–20:12），没有跑满 90 分钟窗口：长测被脚本判定提前终止，而按用户要求本轮不重启新测试。
- 日志中启动序列的来源（真实重新打印 / 缓冲滞留）未查清，长测脚本在"刚复位"状态下的误判也未修复。这会影响下一次复现实验的启动方式，需要先解决。
- 诊断改动仍在两棵树上（nuttx `1b5e71d2f87`、竞赛仓 `e1a459b`），按用户要求本轮不回退。板端运行的就是这一版诊断固件。
- `CONFIG_DEBUG_FREEZE_DIAG` 只在本轮诊断分支打开；合并到公共仓前必须移除或默认关闭。

## 六、原始文件

| 文件 | 说明 |
|---|---|
| `host-soak.log` | 两次运行的完整长测日志（含失败判定与两轮 CYCLE） |
| `sanity-1.log` / `sanity-2.log` | 正常状态下的两次只读 halt 采样 |
| `sanity.gdb` | 上述采样脚本 |
| `build-full.log` | 首次全量构建（`--clean`） |
| `build-after-commit.log` | 提交后增量重建 |
| `flash.log` | 烧录与校验输出 |
| `launch-soak.ps1` / `start-openocd.ps1` | 本轮启动方式 |
| `openocd-start-failure.log` | 第一次后台启动失败（`LIBUSB_ERROR_ACCESS`），随后改为前台服务方式 |

采集脚本本身在 `../qa04-freeze-diag-halt.gdb`。

### 附：本轮遗留文件

- 一个名字被写坏的文件（`-l` 参数里的 `\f` 被解释成换页符），内容就是上面那份 OpenOCD 启动失败日志，已改名移入本目录，工作树不再残留。
- 其余本轮产物都在 `.debug/`（被 `.gitignore` 忽略），已按上表复制到本目录归档。
- 本轮开始前就存在的未提交改动（`VelaGuard_推进方案.md` 删除、10-18 的证据日志、`logs/Foleaf/`）没有动。
- `../nuttx/arch/arm/src/armv7-m/vg_diag.h` 是本轮早期一次未采用的草稿，时间戳 19:40:21。它没有出现在任何 Makefile、Kconfig 或源文件的引用里，`CONFIG_ARCH_VG_DIAG` 不在 `.config` 中，固件里也没有任何 `g_vgdiag_` 符号，因此没有参与本次构建。为保持提交边界清楚，未删除，留待人工决定。

## 七、收尾时的板端与主机状态

收尾时刻（22:36–23:05）的只读检查：

| 项目 | 状态 |
|---|---|
| 板端固件 | `1b5e71d2f87 Oct 19 2026 19:38:59`，烧录后运行；当前状态未验证 |
| 最后一次板端记录 | 20:11:18 `uptime_s=1822`，14/14 在线，`boot_crc=boot_timeout=boot_echo=boot_other=0` |
| COM3 | 收尾时被 `scomm5.15`（PID 18764，22:32:27 启动）占用，`access denied`，**本轮未再读取控制台** |
| RS485 从站 | 20:17:49 自行写出 `STOP requests=79841 replies=79841 crc_scan=4`，进程结束；此后没有总线应答 |
| OpenOCD | 已退出，3333/4444 端口不再监听；调试器连接在 20:11 之后未再使用 |

板端是否仍处于正常运行、是否在 20:17 之后进入冻结，**本轮没有证据**：控制台被占用，从站已停止，没有新的采样。20:17 从站停止的原因没有记录，只能确认它正常写出了 `STOP` 行，不是异常退出。

收尾未执行：烧录、复位、启动新长测、连接调试器。板端保持 20:11 之后的最后状态。

## 八、下一轮开始前要先解决的问题

1. 长测脚本在"刚烧录/刚复位"时会把串口里的启动日志判成运行中重启（本轮第一次运行即因此退出）。需要先排空并确认提示符，或在脚本里区分"启动前缓冲"与"运行中横幅"。
2. 20:11 那条启动序列的来源没有查清：是板端真的重新打印，还是缓冲滞留到那时才被读出。它直接决定这次算不算"计数器未回零但确实重启过"。
3. 控制台被 `scomm` 占用时无法读取板端状态，下一轮开始前要先释放 COM3。
