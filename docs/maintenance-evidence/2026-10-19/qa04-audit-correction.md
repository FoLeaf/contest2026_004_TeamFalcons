# QA-04 既有审计的核对与更正

记录时间：2026-10-19 17:00（Asia/Shanghai）

本文核对 2026-10-19 16:20 至 16:40 两份 QA-04 记录所依赖的 ELF 对应关系、结构布局、异常帧布局和上下文保存约定。原始证据不删改，冲突处以本文为准，并在下文逐条给出可复核的推导依据。

## 一、核对结论摘要

| 项目 | 原记录 | 核对结果 |
|---|---|---|
| 旧镜像与 ELF 对应 | 混用 21:50 构建与 14:01 候选 ELF | 两个 21:50 附近的构建（`18:41:50`、`21:50:34`）代码段地址完全一致；`0x90000000` 起的产品代码可直接解码。板上 23:48:40 镜像没有留存 ELF，地址只能作旁证 |
| TCB 中 `xcp` 偏移 | 按 `+108/+112/+116` 读取 | **错误**。实测 `xcp` 在 TCB 偏移 168，`xcp.saved_regs` 在 168，`xcp.regs` 在 172 |
| `xcp.size` 字段 | 声称 `+108` 是 `xcp.size=1000` | **该字段不存在**。`+108` 是 `adj_stack_size`，`+112` 是 `stack_alloc_ptr`，`+116` 是 `stack_base_ptr` |
| 异常帧布局 | 把 `0x24048018` 起的 8 个字当作 R0..xPSR | **错误**。该处是 `exception_common` 的软件保存区，不是硬件异常帧 |
| idle 的 `xcp.regs` | 被写成 `0x24047ccc` | **不成立**。idle 的 `up_initial_state()` 直接返回，`xcp` 全 0；15:40 的原始样本读到的正是 0 |
| `arm_doirq` 覆盖 `xcp.regs` | 认定为缺陷 | 该赋值与结尾置 NULL 都是上游既有设计，不能单独认定为缺陷 |

## 二、ELF 对应关系

`.debug` 下现存三个 NuttX ELF：

| 文件 | 镜像内构建标识 | 说明 |
|---|---|---|
| `.debug/board-c20177f-before-rebuild/nuttx.elf` | `Oct 18 2026 21:50:34` | 21:50 构建 |
| `.debug/nuttx.elf`（本轮已重建） | `Oct 19 2026 16:53:33` | 本轮候选 |
| `contest2026_004_TeamFalcons/.debug/nuttx.elf` | `Oct 18 2026 18:41:50` | 原工作区旧构建 |

板上运行过的 `23:48:40` 镜像的 ELF 已被后续构建覆盖，仓库内没有留存。核对时用 `arm-none-eabi-nm` 对比 `18:41:50` 与 `21:50:34` 两个构建：

```text
24000264 D g_readytorun       24003d58 B g_running_tasks   24003d5c B g_idletcb
24007344 B g_system_ticks     90000c34 T modifyreg32        90000de8 T exception_direct
90000e20 T arm_doirq          90001008 T exception_common   90001494 T up_trigger_irq
90001534 T up_schedule_sigaction                            900015b4 T arm_sigdeliver
90001640 T nx_start
```

两者完全一致，且 `0x90000000` 是 QSPI XIP 固定基址。因此原记录中 `0x9000177f`、`0x90000c50`、`0x90000de8` 这类产品代码地址的解码可用；RAM 侧结构偏移不能用推测值，必须按本轮实测。

## 三、实测结构布局

用真实头文件编译偏移探针（`arm-none-eabi-gcc` 加 `.config` 生成的 `include/nuttx/config.h`），读 `.rodata` 得到：

```text
offsetof(struct tcb_s, xcp)        = 168
offsetof(struct tcb_s, xcp.regs)   = 172
offsetof(struct tcb_s, xcp.saved_regs) = 168
offsetof(struct tcb_s, name)       = 176
sizeof(struct tcb_s)               = 228
sizeof(struct xcptcontext)         = 8
XCPTCONTEXT_SIZE                   = 216
```

对应反汇编，`arm_doirq` 里 `str.w r1, [r4, #172]` 写的就是 `xcp.regs`，`ldr.w r3, [r4, #140]` 是 `sigdeliver`，`ldr.w r3, [r4, #168]` 是 `xcp.saved_regs`。`XCPTCONTEXT_SIZE=216` 已包含 FPU 的 18 个硬件寄存器与 16 个软件寄存器：`CONFIG_ARCH_FPU=y`、`CONFIG_ARCH_DPFPU=y`、`CONFIG_ARCH_ARMV7M`，没有 `CONFIG_ARMV8M_STACKCHECK_HARDWARE`，没有 `CONFIG_LIB_SYSCALL`、`CONFIG_ARCH_ADDRENV`、`CONFIG_SMP`。

按这个布局重读原记录引用的 TCB 转储：

```text
0x24003d5c <g_idletcb>:      ...
0x24003dbc <g_idletcb+96>:   0x00000000 0x00000000 0x00000000 0x000003e8
0x24003dcc <g_idletcb+112>:  0x24047ccc 0x24047ce4 0x00000000 0x00000000
```

- `+108` = `0x3e8` = 1000 = `adj_stack_size`，正是 `CONFIG_IDLETHREAD_STACKSIZE`
- `+112` = `0x24047ccc` = `_ebss` = `stack_alloc_ptr`
- `+116` = `0x24047ce4` = `stack_base_ptr`

原记录把这三项分别读成 `xcp.size`、`xcp.regs`、`xcp.saved_regs`，由此得出的「idle 上下文指向栈底」「saved_regs 是随机字节」都不成立。

## 四、idle 的 xcp 本来就是全 0

`arch/arm/src/armv7-m/arm_initialstate.c` 对 idle 线程提前返回：

```c
memset(xcp, 0, sizeof(struct xcptcontext));
if (tcb->pid == IDLE_PROCESS_ID)
  {
    tcb->stack_alloc_ptr = (void *)(g_idle_topstack - CONFIG_IDLETHREAD_STACKSIZE);
    tcb->stack_base_ptr  = tcb->stack_alloc_ptr;
    tcb->adj_stack_size  = CONFIG_IDLETHREAD_STACKSIZE;
    return;
  }
```

所以 idle 的 `xcp.regs` 与 `xcp.saved_regs` 初始都为 0。15:40 的原始样本与之一致：

```text
IDLE_REGS_POINTER
0x24003e08 <g_idletcb+172>:	0x00000000
SAVED_REGS_POINTER
0x24003e04 <g_idletcb+168>:	0x00000000
```

冻结期间 `xcp.regs` 始终为 0，不是被覆盖成栈指针。

## 五、异常帧布局的更正

原记录把 `0x24048018` 起的 8 个字读成硬件异常帧。按 `exception_common` 的实际保存顺序，该位置是软件保存区：

| 地址 | 槽位 | 值 |
|---|---|---|
| `0x24048018` | `REG_R0` | `0x00000000` |
| `0x2404801c` | `REG_R1` | `0xffffffe9` |
| `0x24048020` | `REG_R2` | `0x90000c50` |
| `0x24048024` | `REG_R3` | `0x21000036` |
| `0x24048028` | `REG_R12` | `0x00000000` |
| `0x2404802c` | `REG_R14` | `0x00000000` |
| `0x24048030` | `REG_R15` | `0x9000177f` |
| `0x24048034` | `REG_XPSR` | `0x240480cc` |

真正的硬件异常帧在其后 8 个字，即 `0x24048038` 起，原记录没有引用这一段。可确认的事实是：被抢占位置 `0x9000177f` 位于 `nx_start()` 的空闲循环（`0x9000177e: bl up_idle`、`0x90001782: b.n 0x9000177e`），即当时确实在 idle 线程。原记录「LR=0」在更正后同样成立，但它是软件保存的 `REG_R14`，不是硬件帧的 LR 槽。

## 六、`arm_doirq` 的赋值与置 NULL 是上游既有设计

```c
/* This judgment proves that (*running_task)->xcp.regs
 * is invalid, and we can safely overwrite it.
 */
if (tcb != NULL)
  {
    tcb->xcp.regs = regs;
  }
...
/* (*running_task)->xcp.regs is about to become invalid
 * and will be marked as NULL to avoid misusage.
 */
(*running_task)->xcp.regs = NULL;
```

两处分别来自上游提交 `87403477caf`（`arm: remove g_running_tasks[this_cpu()] = NULL`）与 `93f8aedec4f`（`arch: set (*running_task)->xcp.regs to NULL when exit from irq/exception`），并配有说明注释。它们共同表达的约定是：`xcp.regs` 只在被中断的瞬间有效，退出中断后置 NULL 表示失效。因此不能把赋值本身或 `exception_direct` 本身直接认定为缺陷。

外设中断走 `exception_direct`、PendSV 与 SysTick 走 `exception_common`，也是 `arch/arm/src/arm_m/arm_vectors.c` 的既有设计：

```c
[2 ... NVIC_IRQ_PENDSV] = &exception_common,
[(NVIC_IRQ_PENDSV + 1) ... (15 + ARM_PERIPHERAL_INTERRUPTS)] = &exception_direct
```

`exception_direct` 以 `context=NULL` 分发，外设驱动的 ISR 本来就不使用 context 参数。

## 七、上游已有针对同一现象形态的修复

上游提交 `d671bebf24e` 的说明与本轮观察到的「信号投递期间上下文被覆盖、无法回到线程」高度相似：

```text
arch/arm-m: fix when rtcb->sigdeliver inprogress savedregs overlap.

When first rtcb->sigdeliver in process in thread. High priority thread
sched, and cause a new pendsv, at this time, the arm_doirq will trigger a
new up_schedule_sigaction, and cause the signal in-process regs overlay
the saved_regs. And not able to back to the thread again.

Fix by only delivery when saved_regs NULL.
```

该修复已在板上镜像内：反汇编 `arm_doirq` 在调用 `up_schedule_sigaction` 前先检查 `[r4,#140]`（`sigdeliver`）非零、`[r4,#168]`（`saved_regs`）为零。这说明当前镜像已经带有这一层保护，冻结不能用「该修复缺失」解释。

## 八、对后续结论的约束

1. 后续所有 TCB 读取一律使用 `+168`（`saved_regs`）与 `+172`（`regs`），不再使用 `+108/+112/+116`。
2. 异常帧解码必须先区分软件保存区与硬件帧，再谈 R0..xPSR。
3. 优先级被改写的采样单独标记。15:52 至 15:54 的三次 SHPR3 写入发生在 `0xE000ED20`，写前原值 `0x80800080`，此后样本中该寄存器为 `0x80000080`，属于非默认运行条件。本节引用的 15:35 至 15:40 样本早于该写入，SHPR3 仍为 `0x80800080`，不受影响。
4. 原记录中「idle 上下文被覆盖」「`g_running_tasks` 从未离开 idle」两条事实本身仍然成立，但它们的成因需要按更正后的布局重新推导，不能沿用原记录的解释。

## 九、可复核的推导材料

```text
偏移探针源码        /tmp/tcbprobe.c（本轮临时文件，未入库）
偏移探针命令        arm-none-eabi-gcc -c -I. -Iinclude -Iarch/arm/include ...
反汇编对象          .debug/nuttx.elf（本轮构建，符号地址与 21:50 构建一致）
结构定义            arch/arm/include/arm_m/irq.h
初始状态            arch/arm/src/armv7-m/arm_initialstate.c
异常入口            arch/arm/src/arm_m/arm_exception.S
向量表              arch/arm/src/arm_m/arm_vectors.c
中断入口            arch/arm/src/armv7-m/arm_doirq.c
信号投递            arch/arm/src/arm_m/arm_schedulesigaction.c、arm_sigdeliver.c
```
