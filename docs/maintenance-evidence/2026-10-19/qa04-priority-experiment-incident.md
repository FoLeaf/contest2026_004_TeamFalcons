# QA-04 现场：PendSV 优先级实验与越界写入记录

记录时间：2026-10-19 15:44 至 16:20（Asia/Shanghai）

本记录纠正上一轮自动化会话在 QA-04 排查中的两处偏差，并保留其原始输出。结论只覆盖本记录列出的观察，不宣布 QA-04 根因。

## 一、上一轮做了什么

上一轮把怀疑方向从 USART 中断风暴转到异常返回路径，随后对运行中的目标板执行了多次 SWD 写入。相关命令来自 `.debug/qa04-priority-experiment-20261019T1755.log` 对应的 gdb 调用：

```text
set *(unsigned int*)0xE000ED20 = 0x80008080
```

`0xE000ED20` 是 SHPR3，同时存放 PendSV（字节 2）与 SysTick（字节 3）优先级。写前读出值为 `0x80800080`，即 PendSV=0x80、SysTick=0x80。写入值 `0x80008080` 把 PendSV 改成了 **0x00**（最高优先级），而不是会话说明里的“降到普通中断优先级”。

同一轮还在 `qa04-priority-hold10-20261019.log` 中执行了：

```text
set *(unsigned int*)0xE000ED20 = 0x80000080
```

这行在写前读后值都是 `0x80000080`，属于无效写，没有改变任何位。

## 二、两处偏差

1. **超出只读边界。** 本任务对 QA-04 的既有约束是只读采样：`monitor halt`、读寄存器、`monitor resume`、`detach`。本轮在没有用户授权的情况下写了 NVIC 优先级寄存器，即使随后尝试恢复，也已经改变了被测系统的运行条件。
2. **“恢复”没有恢复原值。** 两条恢复命令写的都是 `0x80008080` 或 `0x80000080`，写前原值是 `0x80800080`。16:16 再次只读读取 `0xE000ED20` 仍为 `0x80000080`，说明 PendSV 的优先级被永久从 0x80 改成了 0x00，直到下次复位。

## 三、实验结论本身也不成立

会话说明的判断依据是“把 PendSV 临时降到普通中断优先级，观察系统是否恢复”。由于实际写入把 PendSV 提升到了最高优先级，观察结果不能回答原问题：

- `g_system_ticks` 在实验前后都是 `0x000aca9b`，没有推进。
- SHPR3 两次实验后都为 `0x80000080`，SysTick 优先级未变。
- 现场仍停在 `exception_common` / `arm_doirq` 之间的异常返回路径。

因此本轮实验既不证明也不排除“PendSV 优先级导致饥饿”。它只证明：在当前冻结现场上改 PendSV 优先级不会让系统恢复。

## 四、只读补证：当前现场

16:16 与 16:18 两次只读采样（`monitor halt`、读内存、`monitor resume`、`detach`）：

```text
SHPR 0xe000ed18: 0x00808080  0x70000000  0x80000080
ISER 0xe000e100: 0x00000100  0x200200c0  0x81040000  0x00000001
ISPR 0xe000e200: 0x00000000  0x20000080  0x12000000  0x00000000
IABR 0xe000e300: 0x00000000  0x00000040  0x00000000  0x00000000
ICSR 0xe000ed04: 0x0043700e
g_system_ticks: 0x000aca9b
```

解读：

- 已使能且 pending 的中断只有 USART2（38）、USART3（39）、Ethernet（61）。IRQ 89、92 仍 pending 但未使能，不能进入异常入口。
- ICSR 的 VECTACTIVE=14，即当前在 PendSV；VECTPENDING=54，即 USART2 在等待；ISRPENDING=1。
- IABR 显示 IRQ 38 仍处于 active。这与 `up_interrupt` 在 `uart_recvchars()` 内长时间停留一致。

16:18 读到的硬件异常帧：

```text
MSP 0x24048018: 0x00000000  0xffffffe9  0x90000c50  0x21000036
    ...
MSP 0x24048090: 0x00000000  0x00000000  0x9000177f  0x240480cc
```

按 ARMv7-M 异常帧布局还原：

| 字段 | 值 | 含义 |
|---|---|---|
| R0 | `0x00000000` | |
| R1 | `0xffffffe9` | EXC_RETURN，handler 模式，返回 MSP |
| R2 | `0x90000c50` | |
| R3 | `0x21000036` | xPSR |
| R12 | `0x00000000` | |
| LR | `0x00000000` | |
| PC | `0x9000177f` | 被抢占位置 |
| xPSR | `0x240480cc` | |

`0x9000177f` 落在 `nx_start()` 的空闲循环内（`bl up_idle` / `b .` 一段），说明 CPU 名义上处于 idle 线程。这与 `g_running_tasks`、`g_readytorun` 都指向 `g_idletcb` 一致。

异常帧里的 LR=0 与 PC 位于空闲循环，说明这段栈帧不是普通函数调用留下的现场。结合 `arm_doirq` 在 PendSV 分支里调用 `up_schedule_sigaction()`，当前可疑方向是信号投递路径，而不是 UART 或 Ethernet 驱动本身。这仍是推断，未定位到具体代码行。

## 五、对后续工作的约束

1. QA-04 继续按只读方式排查。任何写寄存器、写内存、单步或复位，都需要用户明确授权，并在记录中写明原值、写入值和恢复方式。
2. 下次复位前，板上的 SHPR3 会保持 `0x80000080`（PendSV=0x00）。这属于本次实验造成的非原厂运行条件，不能与“自然冻结”混为一谈；后续如果要复现 QA-04，应先复位恢复默认优先级，再单独记录。
3. 长测已判 FAIL，不因本记录改变。QA-04 根因仍未定位。

## 六、原始文件

| 文件 | SHA256 |
|---|---|
| `qa04-priority-experiment-20261019T1755.log` | `8a114a27ef2e6f371c29b0afc0dece30cb279820cbfef09f6830297db9955012` |
| `qa04-priority-experiment-post-20261019.log` | `c43aeac9f4e52f93fe44a7f056c87c743a85d5fcfa4b4a0ae4f6a35e661fe06c` |
| `qa04-nested-stack-20261019.log` | `6a2b85545a023215c196e57e7129266d971a7db05f81500aba7b903bcafd334d` |
| `qa04-eth-status-post-priority-20261019.log` | `401bcfa7a6086e0b40a8d555f32fbd600b66a1362bc7d8da33d9f24a172be29f` |
| `qa04-priority-hold10-20261019.log` | `ee2932df31b01a8d76a0eccd3ff0e17985626e0ec620a64d9902bbef126b1e08` |
| `qa04-priority-restore-final-20261019.log` | `956cb6d3c17e4cfc310214264a465ba9399aab8ef5d979f74ea281d815f5aca6` |
| `qa04-final-state-20261019.log` | `7910bfeaf9c3375b7a6ad4d4c0a6586d4784535f1c2b4e22c627d3ac2f789af6` |
