# QA-04 复现记录：候选固件上的第四次存储压力前冻结

记录时间：2026-10-19 18:10（Asia/Shanghai）

本轮完成四件事：把板子从旧镜像的冻结状态恢复到正常基线、构建并烧录含日报修复的候选固件、按原负载组合复验、复现冻结并归档只读现场。**本轮不宣布根因已定位**，QA-04 保持打开。

## 一、已核实事实

### 1.1 旧镜像与 ELF 对应关系

`.debug` 下现存的三个 NuttX ELF 中，`18:41:50` 与 `21:50:34` 两个构建的符号地址完全一致（`g_idletcb=0x24003d5c`、`exception_common=0x90001008`、`arm_doirq=0x90000e20`、`nx_start=0x90001640`），`0x90000000` 是 QSPI XIP 固定基址。板上运行过的 `23:48:40` 镜像没有留存 ELF，因此 RAM 结构偏移必须按本轮实测，不能沿用推测值。详细更正见 [既有审计的核对与更正](qa04-audit-correction.md)。

### 1.2 实测结构布局

用真实头文件编译偏移探针得到：

```text
offsetof(struct tcb_s, xcp)            = 168
offsetof(struct tcb_s, xcp.saved_regs) = 168
offsetof(struct tcb_s, xcp.regs)       = 172
XCPTCONTEXT_SIZE                       = 216
```

`xcp` 内没有 `size` 字段。`+108` 是 `adj_stack_size`，`+112` 是 `stack_alloc_ptr`，`+116` 是 `stack_base_ptr`。`XCPTCONTEXT_SIZE=216` 已计入 FPU 上下文（`CONFIG_ARCH_FPU=y`、`CONFIG_ARCH_DPFPU=y`）。

### 1.3 基线恢复

用 OpenOCD `monitor reset run` 复位后：

| 项目 | 复位后 | 说明 |
|---|---|---|
| `SHPR3`（`0xE000ED20`） | `0x80800080` | 与固件初始化值一致，不再是实验遗留的 `0x80000080` |
| IRQ 32–95 优先级 | 全部 `0x80808080` | 默认值 |
| `g_system_ticks` | 从 0 递增到 `0xef` | tick 恢复推进 |
| COM3 | `uname -a`、`uptime`、`vgpoint list` 均正常 | 14 个已确认点，`committed n=14` |

没有用调试器手工写回任何默认值。

### 1.4 候选固件

| 项目 | 值 |
|---|---|
| 构建命令 | `bash scripts/build.sh`（增量） |
| 镜像内构建标识 | `73ab4a45d Oct 19 2026 16:53:33` |
| 竞赛仓 | `codex/maintenance-closeout`，构建时为 `1b88a07` |
| NuttX | `velaguard/eth-mii-stm32h750b-dk`，`2373ab4a45d` |
| apps | `velaguard/netinit-esp8266`，`0ea954b79` |
| ai_agent | `velaguard/llm-tls-send-retry`，`7dc0a88` |
| MQTT-C | `velaguard/mqtt-pal-hook`，`4dbb7f3` |
| `nuttx.elf` SHA256 | `520331debc6536c281ac1040f13da9528ce25f1a2c7892f884af28a4ec8b820a` |
| `nuttx.bin` SHA256 | `bc2eb8c2c0ceb37f6b032cf36d861527ee7ddcd4225e41a1586c9b85d14a06ca` |
| `qspi_bootstub.bin` SHA256 | `9e6ddfa058915cdb3fec18ff8f862a1ca7dfe2b6a31134d1bc31df037fc2026d` |

日报修复 `22a9bfe` 已确认编入产物：反汇编 `vg_ui_backend_acq_start` 首条调用即 `bl 0x90013fd8 <ensure_file_worker_started>`。

烧录后板端 `uname -a` 返回 `NuttX 0.0.0 2373ab4a45d Oct 19 2026 16:53:33 arm stm32h750b-dk`，与构建标识一致。启动后 14 点在线，总线 `boot_total=194 boot_ok=194`，超时与 CRC 均为 0。

### 1.5 复现结果

长测参数保持原失败场景：COM3、`IntervalSec=900`、`StoreRounds=50`、`MinimumOnline=14`、`CommandTimeoutSec=240`。超时阈值有依据：`vgstoretest 10` 实测 25.95 s，50 轮历史耗时 117–120 s。

| 轮次 | `vgstoretest 50` 耗时 | 结果 |
|---|---|---|
| 1 | 118.84 s | OK |
| 2 | 119.48 s | OK |
| 3 | 117.62 s | OK |
| 4 | 超时 240 s | FAIL |

`RUN_END status=FAIL time=2026-10-19T17:51:11`。冻结发生在开机约 40 分钟后，**早于**旧镜像的第 8 轮，说明日报后台线程启动后的候选固件没有改善该问题。

### 1.6 冻结现场的只读事实

全部采样均为 `monitor halt`、读内存与寄存器、`monitor resume`、`detach`，没有写寄存器、写内存、单步或复位。

| 观察项 | 值 |
|---|---|
| `g_system_ticks` | `0x0003a897`，多次采样不变 |
| `SYST_CSR` / `SYST_RVR` | `0x00010007` / `0x003d08ff`（4,000,000，10 ms） |
| `SYST_CVR` | 每次采样都变化 |
| `CFSR` / `HFSR` | 均为 0，不是自然 HardFault |
| `SHPR3` | `0x80800080`，默认值 |
| IRQ 32–95 优先级 | 全部 `0x80808080` |
| `AIRCR` | `0xfa050000`，PRIGROUP=0 |
| SysTick 向量（`g_irqvector[15]`） | `0x9000b845`，即 `stm32_timerisr` |
| `g_running_tasks` / `g_readytorun` | 都指向 `g_idletcb`（`0x24003d5c`） |
| idle `xcp.saved_regs` / `xcp.regs` | 均为 0 |

ICSR 连续 40 次采样，VECTACTIVE 只在 14（PendSV）与 54（USART2）之间切换，VECTPENDING 只在 54 与 55（USART3）之间切换，**没有一次出现 15（SysTick）**。

USART2 的硬件状态在一次采样中为 `CR1=0x2000002d`（`UE|RE|TE|RXNEIE|FIFOEN`）、`ISR=0x0de210f8`，即 `ORE|IDLE|RXNE|TC|TXFNF|TXFE|RXFF|RXFT` 同时置位，而 `ISR[2:0]` 的 RX FIFO 电平为 0；软件接收缓冲（`g_usart2rxbuffer`）内容在 1 秒前后两次采样完全一致，为 ESP8266 的 AT 响应文本。

## 二、已撤回的错误解释

### 2.1 scene18 的地址归属推断（本轮）

`scene18.gdb` 里写的是：

```text
printf "S%02d pc=%08x sp=%08x lr=%08x ipsr=%d\n", $i, *(unsigned int*)$pc, $sp, *(unsigned int*)$lr, ...
```

`*(unsigned int*)$pc` 与 `*(unsigned int*)$lr` 读的是 PC 和 LR 所指向**地址处的数据**，不是寄存器本身的值。据此得出的「PC 恒为 `0x8005f3ef`、位于内部 Flash、LR=0」全部撤回。原始日志 `scene-18.log` 保留，不删改。

同一轮中从该推断延伸出的「CPU 停在内核外的地址」「可能跳转到 boot stub 区域」等说法一并撤回。`scene19.log` 直接读取 `info registers pc sp lr msp psp`，显示 PC 位于 `exception_common` 内，LR 为 `0xffffffe9`，没有异常。

### 2.2 更早的审计错误（已单独更正）

| 原说法 | 实际情况 |
|---|---|
| `+108` 是 `xcp.size=1000` | 该字段不存在，`+108` 是 `adj_stack_size` |
| idle 的 `xcp.regs` 是 `0x24047ccc` | idle 的 `up_initial_state()` 提前返回，`xcp` 全 0 |
| `0x24048018` 起是硬件异常帧 | 那是 `exception_common` 的软件保存区，硬件帧在其后 8 个字 |
| `arm_doirq` 的 `xcp.regs` 赋值是缺陷 | 上游既有设计，配注释说明，结尾置 NULL 同理 |

## 三、尚待验证的假设

以下都**不是**已定位的根因，只用于指导下一项验证。

### 假设 A：SysTick 因同级中断持续占用而长期得不到服务

依据：ICSR 采样中 SysTick 从未成为 active 或 pending；`SYST_CVR` 在走但 `g_system_ticks` 不变；所有中断（含 SysTick 与 USART2）优先级都是 `0x80`，PRIGROUP=0，同级之间不能互相抢占。

反证与缺口：同级中断本应在当前处理函数返回后按 pending 顺序获得服务；采样是离散的，可能漏掉短小的 `stm32_timerisr`；USART2 的 pending 位在采样间有出现与消失，说明它的处理函数确实在运行。

### 假设 B：USART2 处于 FIFO 标志与数据不一致的异常状态，反复触发中断

依据：`ISR` 同时置 `RXNE`、`RXFF`、`RXFT`、`ORE`、`IDLE`，而 RX FIFO 电平为 0；软件接收缓冲内容不变化。

缺口：`up_rxavailable()` 只检查 `RXNE` 位，`up_receive()` 会读 RDR 并清 `priv->sr`；没有抓到 `up_interrupt` 内部的单步证据，也没有确认 `priv->ie` 的实际值。

### 假设 C：两者共同作用

USART2 的高频中断把 CPU 占满，SysTick 因为同级不能抢占而长期饥饿，进而 `g_system_ticks` 停住、看门狗与调度都不再运行。这条能把 1.6 的所有事实串起来，但目前只有相关性，没有因果证据。

## 四、下一项最能区分假设的验证

按区分能力排序，建议只做第一项：

1. **给 SysTick 单独提高优先级后复验**（不改公共树也可先在运行中验证，但本轮禁止写寄存器，因此应改为在实验分支上编译）。若把 `up_prioritize_irq(STM32_IRQ_SYSTICK, NVIC_SYSH_HIGH_PRIORITY)` 打开后冻结消失或 `g_system_ticks` 恢复推进，则支持假设 A 与 C；若冻结照旧，则 A 被削弱，应转向 B。
2. **在 `up_interrupt` 入口与 `uart_recvchars` 出口各加一个只读计数器**，冻结后读计数器差值，直接判断 USART2 中断是否在被反复进入。
3. **抓一次 `priv->ie` 与 `sr` 的配对快照**，确认 TC/IDLE 分支是否在每次中断都改写中断使能。

这些都需要改公共树或加临时探针，按本轮约束一律未执行。

## 五、原始文件

| 文件 | SHA256 |
|---|---|
| `host-soak.log` | `0b42bb4dd380e009135e0b90b548e9ffd78d627a384b74364e885ee0a93df674` |
| `build.log` | `fb91b87f428836ae587801f1b976a5582af02ea55866b7142839c0b142c98103` |
| `build-source-manifest.json` | `0f8f2d82fdbb3b6621cba2c3b7dfda3597927c4629bf14702f6f261b9002a91b` |
| `reset-and-verify.log` | `754653b27655e83c1f1ccd48570309eb651a40e375109ba819439ad05c58d496` |
| `scene-1.log` | `83d053bdf085d032cb4c61998138c800a019cc962c798e67a2b1cc472a84523a` |
| `scene-2.log` | `79d75b140505b585f5df808ca00072d1a605f9d3f1c6febd4393586b13c10e49` |
| `scene-3.log` | `9b0917a653c1f4cca28c97ca07da7620d6d3ada8c2d46a60a7fb6560e0e29ff2` |
| `scene-4.log` | `c919688c915b7a19d52b743047f9b1bf62c9f067ed132ee0aa48738818a0fec2` |
| `scene-7.log` | `52281ff29cd532cf48f477864a8910f3fd3eb3de9aa2b060143581295e1ffcc1` |
| `scene-8.log` | `361226e21de74f6ba7c6d5af3ac0da3c896ee7fb35ef294a57d8e88a18193d9b` |
| `scene-10.log` | `2dcb87db005699c084e056b4a11d5526462dad34d08f8eb83db7b3154e659d6c` |
| `scene-11.log` | `23e0a1bc10bc108f1b2db67f05831dd8335c7a7c03ea70529af0f0426045cbb3` |
| `scene-13.log` | `44510856d94f8aca4365552f10c8496da6913962f6eb47c27bae1b1f0334416c` |

`scene-5.log`、`scene-6.log`、`scene-9.log`、`scene-12.log`、`scene-14.log` 至 `scene-22.log` 的部分输出直接打印在终端，没有单独落盘；对应的 `.gdb` 脚本已一并归档，可按脚本复跑。
