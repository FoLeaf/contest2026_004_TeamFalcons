# BVO-03b 板载麦克风原始 PDM 采集验证

- 日期：2026-10-21
- 任务：BVO-03b，BVO-03 的第一步，只验原始位流，不做抽取
- 状态：**原始 PDM 采集已通**，静音条件下重复启停稳定；说话条件未验证
- 归属：`../nuttx`（驱动与板级）、本仓（诊断工具与 defconfig）
- 板卡：STM32H750B-DK，B-01

## 1. 结论先行

麦克风能出数据，且数据不是常量线。`/dev` 上没有任何音频设备，采集走的是板级持有
的 SAI1 设备句柄，由 `vgpdmdump` 直接投递接收。

三个必须同时满足的条件，事后看缺一不可：

1. SAI 的 PDM 模式要求 **NODIV=1**，分频器直接驱动位时钟而不是 MCLK。
2. **DMA 必须按 8 位宽度**读 DR。按 16 位读会让相邻字节成对重复（实测重复率
   51.8%，随机水平是 3.2%）。
3. DMA 缓冲区不能落在 **DTCM**。DTCM 对 DMA1/DMA2 不可达，落在那里表现为
   TEIF，看起来像外设故障。

## 2. 硬件核对

### 2.1 麦克风型号

资料包里两个来源对不上：

| 来源 | 型号 |
|---|---|
| `mb1381-h750xb-b01-schematic.pdf`（B-01 发布版） | `MP34DT01TR-M` |
| `mb1381-bdp/.../Audio.SchDoc`（工程源文件） | `IMP34DT05TR` |

按用户 2026-10-21 决定，以 B-01 发布版 PDF 为准。两者都是 ST 的 PDM 数字麦克风，
协议一致，本轮的采集链路对型号不敏感；差异留在 `docs/maintenance-todo.md`。

### 2.2 SW2 与麦克风选通

ST 的 `audio_record.c` 在屏幕上写 "Make sure the SW2 is in position PDM"，但这块板
的 B-01 原理图里**没有 SW2**。逐张 SchDoc 搜索器件位号，只有两个按键：
`B1`（蓝色，USER2）、`B2`（黑色，USER1），没有 PDM/CODEC 选择开关。

结论：STM32H750B-DK 的麦克风直连 SAI4，无需拨动任何开关。那条提示是从
STM32H743I-EVAL 等带音频编解码器的板子继承下来的，与本板无关。本轮未要求
用户拨动任何开关，与实测一致。

### 2.3 引脚与通道

| 网络 | 引脚 | 复用 | 说明 |
|---|---|---|---|
| `MIC_CLK` | PE5 | SAI4_CK2 (AF10) / SAI1_CK2 (AF2) | 麦克风时钟，由 SAI 驱动 |
| `MIC_DATA` | PE4 | SAI4_D2 (AF10) / SAI1_D2 (AF2) | 麦克风位流，进 SAI |

两个网络都直接连到 MCU，不经过 WM8994。PE4/PE5 在 SAI1 与 SAI4 上的角色相同，
所以选哪条路线只改 GPIO 复用号，不改接线。

## 3. 路线选择：SAI1 + DMA1

选 **SAI1 block A + DMA1**，理由是可支配的缓冲区内存：

| | SAI1 | SAI4 |
|---|---|---|
| 所在域 | D2 | D3 |
| 服务它的 DMA | DMA1/DMA2 | 只能 BDMA |
| 缓冲区可达区域 | AXI SRAM、SRAM123、SDRAM | **只有 SRAM4**（64 KB） |
| 额外配置 | 无 | `CONFIG_STM32H7_BDMA` 依赖 `CONFIG_EXPERIMENTAL` |
| ST 参考 | 参考少 | BSP 完整 |

SAI4 的限制是硬的：BDMA 的 `dma_capable` 只接受 SRAM4。产品后续要做连续录音与
抽取滤波，缓冲区必须能放进通用堆，因此选 SAI1。代价是失去 ST BSP 的直接参考，
本轮因此多做了几次寄存器级诊断。

SAI4 保留编译覆盖（BVO-03a 已验证），不参与本轮调试。

## 4. 时钟

### 4.1 PLL2 重配

原 `board.h` 的 PLL2 是 M=5/N=160/P=2，PLL2P=400 MHz。按 MCKDIV 6 位上限 63
算，16 kHz 需要分频 98，写不进去。

改为与 ST 参考同源的一组参数：

```
HSE 25 MHz ÷ PLL2M 25 = 1 MHz
1 MHz × PLL2N 344      = 344 MHz VCO
344 MHz ÷ PLL2P 7      = 49.143 MHz
```

同时把 `PLL2RGE` 从 `4_8_MHZ` 改为 `1_2_MHZ`：分频比 25 让 PLL 输入落在 1 MHz，
旧档位声明的是 4–8 MHz，不改会违反 PLL 输入范围约束。这一点 BVO-03a 复核时漏了。

`PLL2Q`/`PLL2R` 保持原值；两者的输出使能位本来就没置位，不产生时钟。

### 4.2 受影响的时钟域

逐个核对当前构建里 PLL2 的使用方：

| 外设 | 实际时钟源 | 是否受 PLL2 改动影响 |
|---|---|---|
| SAI1（本轮新增） | PLL2P | 是，设计目标 |
| ADC1/2/3 | `D3CCIPR.ADCSEL = PLL2` | 未启用，无影响 |
| FMC | PLL1Q | 否 |
| SDMMC | PLL1Q | 否 |
| QSPI | HCLK | 否 |
| LTDC | PLL3 | 否 |
| SPI1/2/3 | PLL1Q | 未启用 |
| USART/I2C 等 | APB | 否 |

`CONFIG_STM32H7_ADC1/2/3` 在 `.config` 里均为未设置。ADC 是除 SAI 外唯一挂在
PLL2P 上的外设，未启用所以不受影响；若将来启用 ADC，需按新的 PLL2P 重算采样时钟。

### 4.3 实际分频与位率

```
MCKDIV = 49.143 MHz / (16 kHz × 256) = 12
```

读回 CR1 确认 `MCKDIV=12`、`NODIV=1`。

实际进内存的字节率由实测反推：32768 字节耗时 260 ms，约 126 KB/s。与
`PLL2P / 12 / 32 × 1 字节` = 128 kHz 帧率、每帧 1 字节吻合。128 KB/s ÷ 8 =
16 kHz 采样，正是 64 倍抽取后的目标采样率。

**无仪器实测**：本轮没有示波器或逻辑分析仪，位时钟 4.095 MHz 与帧率 128 kHz
是从寄存器值和字节率反推的，不是直接测量。

## 5. 三个实质缺陷及修法

### 5.1 NODIV 必须置位

ST 的 `MX_SAI4_Block_A_Init` 里写 `NoDivider = SAI_MASTERDIVIDER_DISABLE`，
而 `SAI_MASTERDIVIDER_DISABLE` 的定义就是 `SAI_xCR1_NODIV`。BVO-03a 把这个
名字读反了，驱动里一直清零 NODIV。

PDM 必须让分频器直接驱动位时钟。NODIV=0 时 MCLK 与位时钟的关系由帧长另算，
麦克风拿不到时钟。

修法：`sai_mckdivider()` 在 `priv->pdm` 时置 NODIV。

### 5.2 DMA 宽度必须是 8 位

按 16 位读 DR 时，相邻字节成对完全相同：

```
00 00 fe fe ff ff ff ff fe fe 6f 6f a5 a5 96 96
```

`byte dup 51.8%`，而半字之间只有 3.5% 重复（随机水平），说明每对重复字节是同一个
采样的高低两半，高位是低位的重复。

改成 8 位后：

```
00 fe ff ff fe 6f a5 96 a5 d6 a8 56 65 69 55 96
```

`byte dup 3.4%`，回到随机水平。位流变成独立的字节序列。

修法：PDM 接收用 `SAI_RXDMA8_CONFIG`，`unit = 1`，cache 失效范围同步改成按字节计算。

### 5.3 DMA 缓冲区不能在 DTCM

诊断快照抓到 `sm0ar=0x2001c360`，落在 DTCM（0x20000000–0x2001ffff）。DMA1/DMA2
不可达，表现为 TEIF。

这个缺陷是**间歇性**的：堆跨多个内存区，分配器挑能放下的那个，同一命令时好时坏。
本轮前面几次采集成功纯属运气——缓冲区恰好落在 AXI SRAM。

两层防护：

1. defconfig 加 `CONFIG_STM32H7_DTCMEXCLUDE=y`，DTCM 不再进堆（128 KB）。
2. 驱动在 `sai_dma_setup()` 里检查地址，落在 DTCM 时返回 `-EFAULT` 并打印地址，
   而不是让它变成 TEIF。

修后堆总量从 7012624 降到 6881552，差值正是 131072（128 KB）。

### 5.4 停止后未关闭 SAI

原实现在传输完成后没有清 `SAIEN`，SAI 会一直保持启用并持续向麦克风输出时钟，
同时不断请求 DMA 服务。修法：在完成回调末尾清 `SAIEN`，下次传输时重新打开。

## 6. 采集验证

### 6.1 入口

`vgpdmdump`，NSH 命令，参数 `-n` 字节数、`-f` 输出路径、`-q` 只统计、`-x` 十六进制
前缀。工具走板级导出的 `stm32_sai_pdm_device()` 拿到 SAI 设备句柄。

不用 `/dev/i2schar0` 的原因：i2schar 的 `read()` 把缓冲区交给驱动后立即返回，并在
自己的回调里释放缓冲区，数据从不回到调用者。

### 6.2 静音采集

```
vgpdmdump -n 32768 -f /data/pdm-quiet.raw
vgpdmdump: 32768 bytes
  zero=0.0% ones=0.480 entropy=6.345 bit/byte
  min=0x00 max=0xff allff=2
  dup: byte 2.5% (of 32767)  halfword 0.0% (of 16383)
  elapsed=260.0 ms  stream=256.0 ms  rate=123.1 KB/s
```

判读：

- `ones=0.480` 接近 0.5，符合 PDM 位流在无强信号时的占空比。
- `entropy=6.35 bit/byte`，远离 0（常量线）与 8（纯随机）。真实位流因相邻采样相关，
  熵低于 8 是预期。
- `byte dup 2.5%` 与随机水平一致，确认不再是 16 位宽读造成的成对重复。
- `allff=2` 说明没有出现整段 0xFF（数据线被拉死）。

**这些数值证明位流是真实的、在变化的。它们不证明麦克风在响应声音**——常量噪声源
也能产生类似统计。这一点需要说话/静音对照，本轮未做。

### 6.3 重复启停

连续 5 次不同长度采集，`byte dup` 稳定在 2.4%–3.6%，熵 6.28–6.37，无失败：

```
vgpdmdump -q -n 4096   → entropy=6.325  dup=3.2%
vgpdmdump -q -n 4096   → entropy=6.345  dup=3.3%
vgpdmdump -q -n 4096   → entropy=6.354  dup=3.6%
vgpdmdump -q -n 8192   → entropy=6.352  dup=2.6%
vgpdmdump -q -n 16384  → entropy=6.367  dup=2.4%
```

内存前后对照：

```
采集前  6881552  used 311888
采集后  6881552  used 311680
```

总量不变，已用量回落。采集路径无持续资源增长。

### 6.4 并行功能回归

采集期间与采集后：

| 功能 | 状态 |
|---|---|
| 点表查询 `vgpoint get` | `OK cmd=get table=committed n=14` |
| Modbus 周期采集 | `vgstats` 计数在推进（slave=1 total 16→26） |
| eMMC 文件写入 | `/data/pdm-quiet.raw` 等三份文件正常 |
| LVGL / HMI | 正常启动，`vghmi: live ok` |
| 网络 | `vgnet: mqtt online` |

Modbus 全部 timeout 是因为没有连接从站，属于当前环境状态，不是采集引起的回归。

## 7. 改动清单

### nuttx（分支 `velaguard/ptt-user1-stm32h750b-dk`）

| 文件 | 内容 |
|---|---|
| `arch/arm/src/stm32h7/stm32_sai.c` | PDM 用 NODIV=1；PDM 接收改 8 位 DMA；DTCM 地址检查；完成后清 SAIEN；CKSTR 置位；传输快照用于诊断 |
| `arch/arm/src/stm32h7/stm32_sai.h` | `stm32_sai_staterecord()` |
| `boards/.../stm32h750b-dk/include/board.h` | PLL2 重配为 M=25/N=344/P=7；RGE 改 1–2 MHz；`stm32_sai_pdm_device()`、`stm32_pdm_staterecord()` 声明 |
| `boards/.../stm32h750b-dk/src/stm32_bringup.c` | 初始化 SAI1 block A，保存设备句柄并导出 |
| `boards/.../configs/velaguard-lvgl/defconfig` | `STM32H7_SAI1`、`SAI1_A`、`SAI_PDM_MODE`、`DTCMEXCLUDE`、`VG_PDM_DUMP` |

### 本仓

| 文件 | 内容 |
|---|---|
| `app/velaguard/vgpdmdump.c` | 诊断采集命令 |
| `app/velaguard/Kconfig` | `VG_PDM_DUMP` |
| `app/velaguard/Makefile` | 构建接入 |
| `scripts/configs/velaguard-lvgl.defconfig` | 上述 5 个选项 |

## 8. 未验证与下一步

### 未验证

1. **说话/静音对照未做**。本轮没有采集到人声条件下的样本，所以"麦克风响应声音"
   这一条没有证据。位流统计正常只能排除常量线，不能排除恒定噪声。
2. **无仪器实测时钟**。位时钟与帧率是从寄存器值和字节率反推。
3. **未做长时连续采集**。最长一次 32768 字节（约 260 ms），没有跑分钟级连续性检查。
4. **SW2 结论基于原理图，未目视确认板上是否有丝印标 SW2 的器件**。B-01 原理图与
   BOM 里都没有，但物理板未逐一核对。

### 下一步的输入条件

PDM→PCM 抽取滤波需要的输入已经确定：

| 项目 | 值 |
|---|---|
| 数据格式 | 8 位无符号字节流，每字节 8 个 PDM 采样 |
| 位序 | LSB 优先（`CR1.LSBFIRST=1`） |
| 通道布局 | 帧长 32、双槽、只有槽 1 使能，每帧 1 字节 |
| 字节率 | 约 128 KB/s |
| 抽取比 | 64（与 ST 参考一致） |
| 目标采样率 | 16 kHz |
| 数据来源 | 单麦克风 U34，CH2 位流时钟 |

具体到实现，需要注意：

- 抽取滤波不能直接套用 ST 的 `PDM_Filter`：那个库是 MCD-ST Image SW License V2。
- **但"非 Apache-2.0"本身不是自写的充分理由**。下一步应先确认：许可证是否允许
  以二进制形式随固件分发（多数 ST 图像库允许，只限制源码再分发）。如果允许，
  以预编译库形式接入是可行选项，工作量远小于自写 64 倍抽取。这一点需要在
  BVO-03 正式开工前查清许可证原文，不能沿用"不能用"的结论。

### 未处理的已知问题

- 说话条件需要真实人声，本轮由用户决定跳过。BVO-03 验收"人声可辨"必须补上。
- `stm32_sai_staterecord()` 的诊断快照是 bring-up 用的，产品化时应移除或改为
  仅在调试配置下编译。
- 工具走的是板级导出的设备句柄，不是标准字符设备。BVO-03 正式实现时需要决定
  产品音频路径的设备形态（`/dev/audio/pcm0c` 或 `/dev/i2scharN`）。
