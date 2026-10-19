# QA-04 冻结现场：异常帧重复与 idle TCB 上下文被覆盖

记录时间：2026-10-19 16:20 至 16:40（Asia/Shanghai）

本记录只整理冻结现场的可复核事实，不宣布根因已关闭。所有读取均为 `monitor halt`、读内存/寄存器、`monitor resume`、`detach`；本轮没有写寄存器、写内存、单步或复位。板上 SHPR3 仍是上一轮实验遗留的 `0x80000080`，见 [PendSV 优先级实验与越界写入记录](qa04-priority-experiment-incident.md)。

## 一、冻结的直接表现

| 观察项 | 值 | 说明 |
|---|---|---|
| `g_system_ticks`（`0x24007344`） | `0x000aca9b` | 多次采样不变 |
| `SYST_CSR`（`0xE000E010`） | `0x00010007` | SysTick 使能、中断使能、CPU 时钟源 |
| `SYST_CVR`（`0xE000E018`） | 每次采样都变化 | 硬件计数器仍在运行 |
| `SYST_RVR`（`0xE000E014`） | `0x003d08ff` | 10 ms 重装值 |
| `CFSR` / `HFSR` | 0 | 不是自然 HardFault |
| `ICSR`（`0xE000ED04`） | `0x0043700e` / `0x00437836` 交替 | active=PendSV 或 USART2，pending 另一个 |

SysTick 硬件在跑、中断使能，但 `g_system_ticks` 不增加。这说明 SysTick 中断没有进入 `nxsched_process_timer()`，或者进入了但没有执行到 `clock_timer()`。

## 二、异常帧重复

同一进程内连续采样，MSP 窗口固定停在 `0x24047f80`–`0x24048030` 一段，内容在两次采样之间不变：

```text
0x24047f80: 0x00000000 0x00000000 0x00000000 0x24003cd8
0x24047f90: 0x24000264 0x90001063 0x24048070 0x00000000
0x24047fa0: 0x00000000 0x00000000 0x24003cd8 0x24000264
0x24047fb0: 0x00000003 0x00000005 0x00000002 0x00000000
0x24047fc0: 0x00000000 0xfffffff1 0x24003d5c 0x00000000
0x24047fd0: 0x24003cd8 0x24000264 0x00000003 0x00000005
0x24047fe0: 0x00000002 0x00000000 0x00000000 0xffffffe9
0x24047ff0: 0x00000000 0x00000000 0x00000000 0x00000000
0x24048000: 0x00000000 0x00000000 0xe000ed04 0x00437836
0x24048010: 0x10437836 0x00000000 0x00000000 0xffffffe9
0x24048020: 0x90000c50 0x21000036 0x24003d5c 0xffffffe9
0x24048030: 0x00000002 0x24003d5c 0x380003d0 0x00000001
```

`0x24048018` 起是完整的 ARMv7-M 硬件异常帧，`EXC_RETURN=0xffffffe9`（handler 模式、返回 MSP）：

| 槽位 | 值 |
|---|---|
| R0 | `0x00000000` |
| R1 | `0xffffffe9` |
| R2 | `0x90000c50` |
| R3 | `0x21000036` |
| R12 | `0x00000000` |
| LR | `0x00000000` |
| PC | `0x9000177f` |
| xPSR | `0x240480cc` |

`0x9000177f` 位于 `nx_start()` 的空闲循环（`bl up_idle` / `b .` 一段），即被抢占的是 idle 线程。`0x90000c50` 是 `modifyreg32()` 的 `pop {r4, pc}` 返回点，出现在栈上说明该函数曾在异常上下文中执行过。

## 三、idle TCB 的上下文指针

`g_idletcb` 在 `0x24003d5c`。按当前结构布局读取：

```text
0x24003dc8 <g_idletcb+108>: 0x000003e8  0x24047ccc  0x24047ce4  0x00000000
```

- `+108`（`xcp.size`）= `0x3e8` = 1000
- `+112`（`xcp.regs`）= `0x24047ccc`
- `+116`（`xcp.saved_regs`）= `0x24047ce4`
- `+168`（`xcp.saved_regs`，另一套布局）与 `+172`（`xcp.regs`）都为 0

`0x24047ccc` 正好是 `_ebss`，也是 `up_initial_state()` 给 idle 设置的栈顶减 1024。也就是说，idle 的保存上下文指向栈底，而不是它真正被抢占时的栈位置。`0x24047ce4` 处的内容是随机字节（`0x1e08a97f`、`0xc538d305` …），不是有效的异常帧。

同时 `g_running_tasks`（`0x24003d58`）= `0x24003d5c`，即 `g_idletcb`。整个冻结期间它从未变过，说明**从未发生过一次成功的 idle→其他线程上下文切换**。

## 四、可以确认的机制

`arm_doirq()` 在每个异常入口都会执行：

```c
if (tcb != NULL)
  {
    tcb->xcp.regs = regs;
  }
```

`tcb` 取自 `g_running_task`。冻结现场里它是 idle，而实际被抢占的也是 idle，所以每次中断都会把 idle 的 `xcp.regs` 改写成**当前这次中断的栈帧**。PendSV 分支随后调用 `up_schedule_sigaction(tcb)`，后者在 `xcp.saved_regs == NULL` 时会：

```c
tcb->xcp.saved_regs = tcb->xcp.regs;
tcb->xcp.regs      -= XCPTCONTEXT_SIZE;
memcpy(tcb->xcp.regs, tcb->xcp.saved_regs, XCPTCONTEXT_SIZE);
```

一旦 `xcp.regs` 已经被写成“本次中断的栈帧”，这份栈帧就会被复制到 `regs - 216` 并当作要恢复的上下文。异常返回时 `exception_common` 从 `arm_doirq()` 返回的 `regs` 恢复，而这段区域正被后续中断继续复用。

这可以解释：为什么每次采样看到的都是同一段栈窗口、为什么 `g_system_ticks` 不推进、为什么 `g_running_tasks` 永远停在 idle。这是**机制层面的自洽解释**，不是已定位到具体触发条件的根因。

## 五、尚未回答的问题

1. 第一次进入这个状态时，`xcp.saved_regs` 是否本来就应该为 NULL。`arm_sigdeliver()` 末尾会执行 `rtcb->sigdeliver = NULL` 和 `xcp.saved_regs = NULL`，如果信号投递在 idle 上被重复触发，就会反复回到这个分支。
2. `modifyreg32()` 出现在栈上，说明有代码在中断上下文里改外设寄存器。它与 `up_interrupt` 里的 `up_restoreusartint()` 路径吻合，但还没有抓到具体调用者。
3. 冻结前最后一次成功的长测轮次、LLM/网络重连和 `vgstoretest` 之间是否存在共同触发条件，需要单独复现。

在这些问题回答之前，QA-04 保持打开。下一步应当先做**可复现的触发条件**，而不是继续在冻结现场上取更多静态快照。

## 六、可验证的候选修复方向（未实施）

按上面机制，最直接的加固点是让 `arm_doirq()` 在 idle 线程上不要用中断栈帧覆盖 `xcp.regs`，或者在 `up_schedule_sigaction()` 之前确认 `tcb->xcp.regs` 确实属于该 TCB 自己的栈区间。两者都需要先在主机侧或 QEMU 上验证，不能直接上板试。本轮不修改公共树。

## 七、原始文件

| 文件 | SHA256 |
|---|---|
| `qa04-frame-recurrence-20261019T1620.log` | `f4bfae126678f2164a49d4f5e28f718488bea050d81b694647176f9d09fe1286` |
| `qa04-frame-recurrence-20261019T1620.gdb` | `848ab4b10fcc3e477c3c7c37f6b1ac0f56147b7de388933748eb747dbcfa0c44` |

该次采样停在 `up_trigger_irq+42`（`0x900014be`），`lr=0xffffffe9`，即由异常上下文调用，与 `exception_direct()` 在发现 `g_running_task != this_task()` 后触发 PendSV 的路径一致。它同时给出与正文相同的 idle TCB 指针、`g_running_tasks` 和 MSP 窗口。

`qa04-priority-experiment-incident.md` 列出了上一轮实验的原始日志与哈希。
