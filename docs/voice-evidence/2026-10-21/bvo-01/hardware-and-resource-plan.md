# BVO-01 板端语音硬件与资源方案

- 日期：2026-10-21
- 任务：BVO-01，依据 `board-voice-execution-plan.md` 第 5 节与 ADR 0009
- 状态：**方案已出，硬件项待实测**。本文不含任何编译、烧录或板端运行结论。
- 归属：参赛仓文档；未修改 `AGENTS.md`、ADR 0009 与执行方案，二者仍在 `.vg-maintenance-closeout`

## 1. 范围与结论摘要

BVO-01 的任务是用原理图和 ST BSP 交叉核对 USER1 按键、数字麦克风与 Line Out 三条线路，给出总线、时钟、DMA、引脚占用结论，列出 RAM 与线程预算，并固化 BVO-02 至 BVO-05 要用的接口参数。

本轮结论：

| 项目 | 结论 | 强度 |
|---|---|---|
| USER1 按键 | PC13，BSP 与原理图一致，板上无其它占用 | 已核对，按下电平待实测 |
| 数字麦克风 | U34 IMP34DT05TR，PDM 输出，接 PE4/PE5 | 已核对 |
| 麦克风到 MCU 的通路 | 存在两条：SAI4 PDM 直连，或经 WM8994 片内抽取回 SAI2_SDB | **路线待实测确认** |
| Line Out | 绿色 3.5mm 插座是输出，但由 WM8994 哪一路驱动未定 | **待实测** |
| SAI 驱动 | NuttX 无 STM32H7 SAI 驱动，需新增 BVO-03a | 已核实 |
| DMA | SAI2 走 DMA1/DMA2；SAI4 只能走 BDMA，且 BDMA 依赖 EXPERIMENTAL | 已核实 |
| 时钟 | PLL2 已启用，但 `board.h` 中 PLL2 的 Q/R 定义不完整 | 已核实 |
| 内存 | 板端 maxused 428 KB，总 7.0 MB，余量足够 480 KB 原始录音 | 依据 2026-10-18 基线 |

需要实测才能定的项目共 7 条，见 `pre-measurement-checklist.md`。其中 M-01（焊桥）决定采集路线，是本轮唯一的阻断性未知项。

## 2. 证据来源与判读边界

| 来源 | 路径 | 可用性 | 判定强度 |
|---|---|---|---|
| 原理图 PDF | `mb1381-h750xb-b01-schematic.pdf`（22 页） | 文字层无法机器抽取，只有引脚编号等短文本 | 需人工视图 |
| Altium 原理图源 | `mb1381-bdp/MB1381B/Altium_Designer/Audio.SchDoc` 等 | 可提取元件、网络标号、坐标、焊桥类型 | 元件与网络可信；**连接关系需人工判读** |
| ST BSP | `bsp/`（v3.3.4，Board ID MB1381B） | 完整 `.c`/`.h` | 强，代表 ST 实际配置 |
| 引脚分配表 | `STM32H750B-DK_pinmux_unofficial.csv` / `.md` | 文件名含 unofficial | 仅辅助 |
| NuttX 源码 | `../nuttx` | 完整 | 强 |
| 板端基线 | `docs/maintenance-evidence/2026-10-18/board-baseline.md` | 实际运行记录 | 强，但为另一份固件 |

**判读边界**：本文中凡标注为网络标号或元件的结论，来自 SchDoc 文本提取，可信到元件与网络名一级；凡涉及哪个网络连到哪个引脚、焊桥实际短接与否、插座由哪一路驱动，SchDoc 文本不含连线拓扑，只能靠实测确认。这些项目一律标为待实测，不写成已确认。

## 3. 信号级交叉核对表

坐标列为 Audio.SchDoc 中的文本记录坐标，供人工在原理图视图里定位。

| 信号 | 原理图（网络标号/引脚/坐标） | ST BSP 宏 | NuttX H7 pinmap | 结论 | 矛盾 |
|---|---|---|---|---|---|
| SAI2_MCLKA | 网络 `SAI2_MCLKA`，PI4（410,480 / 840,550） | `AUDIO_OUT_SAIx_MCLK_GPIO_PORT GPIOI` `GPIO_PIN_4` `GPIO_AF10_SAI2` | `GPIO_SAI2_MCK_A_2`（PE 无关，PORTI PIN4 AF10） | 一致 | 无 |
| SAI2_SCKA | 网络 `SAI2_SCKA`，PI5（410,530 / 470,530） | `SCK_GPIO_PORT GPIOI` `GPIO_PIN_5` | `GPIO_SAI2_SCK_A_2`（PORTI PIN5 AF10） | 一致 | 无 |
| SAI2_SDA | 网络 `SAI2_SDA`，PI6（410,510 / 470,510） | `SD_GPIO_PORT GPIOI` `GPIO_PIN_6` | `GPIO_SAI2_SD_A_2`（PORTI PIN6 AF10） | 一致 | 无 |
| SAI2_FSA | 网络 `SAI2_FSA`，PI7（410,520 / 470,520） | `FS_GPIO_PORT GPIOI` `GPIO_PIN_7` | `GPIO_SAI2_FS_A_2`（PORTI PIN7 AF10） | 一致 | 无 |
| SAI2_SDB | 网络 `SAI2_SDB`，PG10（470,500） | `AUDIO_IN_SAIx_SD_GPIO_PORT GPIOG` `GPIO_PIN_10` `GPIO_AF10_SAI2` | `GPIO_SAI2_SD_B_4`（PORTG PIN10 AF10） | 一致 | 无 |
| SAI4_CK2 | 网络 `SAI4_CK2`，PE5（130,460 / 180,460） | `AUDIO_IN_SAI_PDMx_CLK_IN_PORT GPIOE` `GPIO_PIN_5` `GPIO_AF10_SAI4` | `GPIO_SAI4_CK2_0`（PORTE PIN5 AF10） | 一致 | 无 |
| SAI4_D2 | 网络 `SAI4_D2`，PE4（130,470 / 180,470） | `AUDIO_IN_SAI_PDMx_DATA_IN_PORT GPIOE` `GPIO_PIN_4` | `GPIO_SAI4_D2_0`（PORTE PIN4 AF10） | 一致 | 无 |
| Audio_Int | 网络 `Audio_Int`，PG7（410,490），原理图标注 `NLAudio0Int` | 无（BSP 不处理 codec 中断） | `GPIO_SAI1_MCLK_A_2` 复用同一引脚，但未使用 | 引脚存在，VelaGuard 未使用 | BSP 未覆盖 |
| I2C4_SCL | 网络 `IIC4_SCL`，PD12（850,590） | `BUS_I2C4_SCL` / `AUDIO_I2C_ADDRESS 0x34` | `stm32_i2cbus_initialize(4)` | 一致 | 触摸、Arduino、STMod 共用 |
| I2C4_SDA | 网络 `IIC4_SDA`，PD13（850,600） | `BUS_I2C4_SDA` | 同上 | 一致 | 同上 |
| MIC_CLK | 网络 `MIC_CLK`（240,320） | 无（BSP 由 SAI4 输出 CK2 作 PDM 时钟） | 无 | 麦克风时钟，来源见 M-01 | **待实测** |
| MIC_DATA | 网络 `MIC_DATA`（280,330） | 无 | 无 | 麦克风数据，去向见 M-01 | **待实测** |
| MICBIAS1 | 网络 `MICBIAS1`（150,150） | WM8994 提供 | 无 | 由 WM8994 供电，是否需软件使能见 M-06 | **待实测** |
| PC13 / WAKEUP | 引脚 `PC13`，网络 `WAKEUP`，文本 `Wakeup & Key Button`、`B1` | `BUTTON_USER_PIN GPIO_PIN_13`，`GPIOC`，`GPIO_PULLDOWN`，`GPIO_MODE_IT_RISING` | `arch/arm/include/stm32h7/stm32h7x3xx_irq.h` 有 EXTI15_10 | PC13 是 USER1，按下为高 | `board.h:527` 注释误写 PI11 |

关于最后一行：`boards/arm/stm32h7/stm32h750b-dk/include/board.h:527` 的注释写的是 `connected to GPIO PI11`，与 BSP、板 README、以及参赛仓 `docs/velaguard-expansion-board.md:274` 的记录都不一致。后三处一致记为 PC13。本文按 PC13 采用，注释错误留给 BVO-02 在改板级文件时一并修正。

## 4. 外设占用冲突表

| 资源 | 用途 | 当前是否被 VelaGuard 占用 | 音频要用的部分 | 冲突判断 |
|---|---|---|---|---|
| SAI2 block A | WM8994 播放，master TX | 否（`CONFIG_STM32H7_SAI` 未启用） | PI4/5/6/7 | 无冲突，独占 |
| SAI2 block B | WM8994 采集，slave RX | 否 | PG10 | 与 block A 同属 SAI2，共享 MCLK/SCK/FS，需同一时钟配置 |
| SAI4 block A | PDM 麦克风，master RX | 否 | PE4/PE5 | 仅路线 A 使用 |
| DMA1/DMA2 | SAI2 数据搬运 | 已启用（`CONFIG_STM32H7_DMA1/DMA2=y`），已有使用者为 SDMMC 等 | 两个通道（TX/RX） | 需 `stm32_dmachannel` 按 DMAMAP 申请，不复用固定 stream |
| BDMA | SAI4 数据搬运 | 否（`CONFIG_STM32H7_BDMA` 未开，且依赖 `EXPERIMENTAL`） | DMA 通道来自 DMAMUX2 | 仅路线 A 需要，额外打开一个实验性选项 |
| I2C4 | WM8994 控制（地址 0x34） | 是，FT5x06 触摸已初始化（`stm32_ft5x06.c:281`） | WM8994 寄存器读写 | 无冲突，`stm32_i2cbus_initialize` 带引用计数（`stm32_i2c.c:2765-2778`） |
| PLL2 | SAI 核时钟潜在来源 | PLL2 已启用（M=5、N=160、P=2）；当前无实际消费者 | SAI2/SAI4 核时钟 | 可用，但 `board.h` 的 PLL2 Q/R 定义不完整，见第 8 节 |
| PLL3 | LCD | 是（LTDC） | 不使用 | 不改动 |
| PC13 | USER1 按键 | 否 | 按键输入 | 无冲突；需打开 `ARCH_HAVE_BUTTONS` |
| PG7 | WM8994 中断 | 否 | 可选 | 首版不需要 codec 中断，保持不使用 |
| PE4/PE5 | 麦克风 PDM | 否 | 路线 A 用 | 与 SAI1 复用同一组引脚（AF2/AF6），本板 SAI1 未用，不冲突 |
| PI4-PI7 | SAI2 | 否 | 播放与采集共用 | 无冲突 |
| PG10 | SAI2_SDB | 否 | 路线 B 用 | 无冲突 |
| SRAM4 | BDMA 可达区 | 否 | 路线 A 的录音缓冲受限在此 64 KB | 路线 B 不受此限制 |
| DTCM/ITCM | — | 已进堆 | 不可用于 DMA 缓冲 | 硬约束，见第 7 节 |

## 5. 两条采集路线

### 路线 A：麦克风直连 SAI4，片外做 PDM 抽取

链路：U34 → MIC_CLK/MIC_DATA → PE5/PE4 → SAI4 block A（master RX，PDM 模式）→ BDMA → SRAM4 → 软件或库做 64 倍抽取 → PCM。

软件工作量：

- SAI4 的 PDM 模式要新写。`arch/arm/src/stm32f7/hardware/stm32_sai.h` 里没有 PDM 相关位定义，H7 寄存器头同样没有，需要按参考手册新增 `CR1.PDMEN`、`CR4.MICPAIRNBR`、PDM 时钟使能等。
- PDM 到 PCM 的抽取滤波要自备。本地资料包里没有 ST 的 PDM2PCM 库（`bsp/` 只有 `.c`/`.h`，无 `Components/`）。可考虑方案是用 `apps/mlearning/cmsis/CMSIS_5/CMSIS/DSP` 里的 `arm_fir_decimate_*`，但 64 倍抽取的滤波器系数、位序与增益标定都要自行设计，并与 PDM 位流对齐。
- 要打开 `CONFIG_STM32H7_BDMA`，而该项 `depends on EXPERIMENTAL`。

约束与风险：

- 录音缓冲必须落在 SRAM4（`0x38000000`，64 KB），因为 BDMA 只能访问 D3 域与 SRAM4（`stm32_dma.c:2048-2160`）。15 秒 16 kHz 单声道原始数据是 480 KB，SRAM4 放不下，必须分段搬运。
- 抽取运算在主核上做，64 倍抽取意味着每输出一个样本要处理 64 个 PDM 位，16 kHz × 64 即约 1 MHz 的位流处理量，对 400 MHz 的 M7 不算重，但要评估对采集周期与 UI 线程的影响。

### 路线 B：麦克风进 WM8994，片内做 PDM 抽取，从 SAI2_SDB 回来

链路：U34 → MIC_CLK/MIC_DATA → WM8994 DMIC 输入（DMICDAT）→ 片内抽取 → AIF1 → SAI2_SDB（PG10）→ SAI2 block B（slave RX）→ DMA1/DMA2 → 任意 SRAM/SDRAM → PCM。

依据：ST BSP `BSP_AUDIO_IN_Init` 的 instance 0 在 `Device == AUDIO_IN_DEVICE_DIGITAL_MIC` 时把 WM8994 配成 `InputDevice=WM8994_IN_MIC2`（`bsp/stm32h750b_discovery_audio.c:1535-1537`），slot 选 `CODEC_AUDIOFRAME_SLOT_13`（`:1463-1465`），SAI2 block B 配成 `SAI_MODESLAVE_RX`（`:1472`）。这是 ST 官方对这颗板子数字麦克风的配置，本身证明路线 B 在硬件上成立。

软件工作量：

- `drivers/audio/wm8994.c` 要新增输入路径：`AUDIO_TYPE_INPUT` 的 getcaps 与 configure、DMIC 相关的 PM/ADCL 寄存器序列、`I2S_RECEIVE` 调用。驱动里现有的输入代码整体在 `#if 0` 内（`wm8994.c:181`、`2222`、`2325`），不能直接启用，要整段新写并按 WM8994 手册逐寄存器核对。
- SAI2 block B 需要一个 slave RX 的 I2S 实例，仍然要 BVO-03a 的 SAI 驱动。

约束与风险：

- 不依赖 BDMA，录音缓冲可以放 AXI SRAM（512 KB）或 SDRAM（6 MB 可用堆），480 KB 上限不成问题。
- 不需要 PDM 抽取库。
- 采集与播放共用 SAI2 的时钟家族，录放要按执行方案互斥，这一点与 SAI2 共享正好一致。
- 风险集中在 WM8994 的 DMIC 配置是否正确。若 M-01 显示麦克风未接 WM8994，路线 B 不成立。

### 取舍

推荐路线 B。理由依次是：不引入 EXPERIMENTAL 依赖、不需要 PDM 抽取库、录音缓冲不受 64 KB SRAM4 限制、与播放共用 SAI2 的互斥模型一致。

路线 B 的主要不确定性是 WM8994 输入路径要新写；但路线 A 要新写的东西更多，包括 SAI4 PDM 模式、PDM 抽取滤波、BDMA 配置三块。

决策规则：M-01 实测麦克风经焊桥进 WM8994 DMIC，则走路线 B，BVO-03 承担 WM8994 输入路径；M-01 显示直连 SAI4，则走路线 A，并把 SAI4 PDM 与 BDMA 单列为 BVO-03a2，PDM 抽取方案另作设计。两条路线都要 BVO-03a 的 SAI 基础驱动。

## 6. 播放路线

链路：PCM/WAV → `pcm_decode` → WM8994（SAI2 block A，master TX）→ 模拟输出 → 绿色 3.5mm 插座。

已确认：

- SAI2 block A 为 master TX，MCLK/SCK/FS/SD_A 在 PI4/5/6/7，BSP 与 NuttX pinmap 一致。
- `drivers/audio/wm8994.c` 当前只启用 HPOUT1（耳机）与 SPKOUT（扬声器）两条模拟输出，代码里 `hp_out=1`、`spk_out=0` 写死（`wm8994.c:1879-1882`），注释明确写 `Currently only headphones are supported!`。
- BSP `BSP_AUDIO_OUT_SetDevice` 的注释写 `there is only one device (AUDIO_OUT_HEADPHONE)`。
- `wm8994.h` 已有 LINEOUT1/2 的使能位与输出混音位定义（如 `WM8994_LINEOUT1P_ENA`、`WM8994_LINEOUT2P_ENA`、`WM8994_DAC1L_TO_MIXOUTL`），但驱动里没有使用。

三个未决点，全部依赖实测：

1. 绿色插座由哪一路驱动。若是 HPOUT1，现有驱动可用；若是 SPKOUT，要改 `hp_out`/`spk_out` 的写死值；若是 LINEOUT1/2，要新增 LINEOUT 路由与使能序列。对应 M-02。
2. 两个 2x1 跳线排针的功能。SchDoc 里只有两个 `Jumper Header 2x1` 元件（坐标 860,270 与 860,300），文本无网络名，无法判断是选择输出通道还是短接某段线路。对应 M-03。
3. 音量安全初值。BSP 默认 `WM8994_INITVOLUME 250`（千分比），Line Out 直连外部设备时这个值可能偏大。对应 BVO-04 实测。

执行方案要求输出到 Line Out，而现有驱动只保证耳机路径。这一点不能直接沿用 BSP 的结论，必须在 BVO-04 里按实测结果补代码。

## 7. RAM、DMA、线程与缓冲预算

### 基线

| 项目 | 数值 | 来源 |
|---|---|---|
| 堆总量 | 7,013,984 B | `board-baseline.md` 的 `/proc/meminfo` |
| 已用 | 317,128 B | 同上 |
| 峰值 | 428,024 B | 同上 |
| AXI SRAM | 512 KB @ `0x24000000` | `scripts/qspi_flash.ld` |
| SRAM4 | 64 KB @ `0x38000000` | 同上 |
| SDRAM | 8 MB @ `0xd0000000`，堆偏移 2 MB | `board.h` `BOARD_SDRAM2_SIZE`/`_HEAP_OFFSET` |
| DTCM | 128 KB @ `0x20000000` | `memory.ld`（本配置未进堆） |

### 音频占用估算（16 kHz、16 bit、单声道、15 秒上限）

| 项目 | 估算 | 说明 |
|---|---|---|
| 原始录音 | 480,000 B | 16000 × 2 × 15 |
| DMA 双缓冲 | 2 × 4096 B = 8192 B | 缓冲大小待 BVO-03 按中断频率定 |
| WAV 封装额外 | 约 44 B | `pcm_decode` 的头部 |
| 播放缓冲（TTS 回复片段） | 按 30 秒音频上限约 960 KB，建议按段落流式送入 | 实际以 TTS 返回格式为准，见 BVO-06 |
| 采集线程栈 | 8192 B | 与现有 `VG_HMI_SCAN_STACKSIZE` 同级 |
| 音频播放线程栈 | 8192 B | 同上 |
| SAI 驱动内部 | 按 F7 蓝本的 `CONFIG_STM32F7_SAI_MAXINFLIGHT` 16 个容器估，约若干 KB | BVO-03a 定具体值 |

余量判断：峰值 428 KB 加上录音 480 KB 加缓冲与栈，总量仍在 7.0 MB 的 15% 以内，余量充足。真正的限制不在总量，而在**位置**：

- DTCM 与 ITCM 不能被 DMA1/DMA2 访问（`stm32_dma.c` 的 `dmacapable` 明确拒绝），录音与播放缓冲不能放这里。
- 若走路线 A，录音缓冲必须落 SRAM4（64 KB），480 KB 的原始音频要分段搬出，不能一次性驻留。
- 若走路线 B，缓冲可放 AXI SRAM 或 SDRAM，无分段约束。

### cache 与 DMA 一致性

- 本板 `CONFIG_ARMV7M_DCACHE=y` 且为写透（`_WRITETHROUGH=y`），`up_enable_dcache()` 在 `stm32_start.c:266` 无条件调用。
- NuttX H7 的 DMA 层不做 cache 维护，`stm32_dma.h` 的回调注释明确要求客户端在 RX 完成后自行 invalidate。
- 因此 SAI 驱动必须在 RX 完成回调里调用 `up_invalidate_dcache`，在 TX 启动前确保数据已写出。缓冲地址按 `ARMV7M_DCACHE_LINESIZE` 对齐。
- 写透模式降低了脏数据风险，但不取消对齐与 invalidate 要求，因为读方向仍可能读到过期行。
- 需要注意，内核对齐检查本身在写透模式下不生效：`stm32_dma.c:2107-2125` 的 `#if defined(CONFIG_ARMV7M_DCACHE) && !defined(CONFIG_ARMV7M_DCACHE_WRITETHROUGH)` 在本板配置下为假，`dmacapable` 不会因为未对齐而拒绝。对齐只能靠驱动自己保证，不能指望内核报错。

## 8. 时钟方案

### 现状

- HSE 25 MHz。PLL1 供 SYSCLK 400 MHz。PLL3 供 LCD。
- PLL2 已在 `board.h` 启用：M=5、N=160、P=2，VCO 800 MHz，P 输出 400 MHz。目前没有实际消费者（ADC 未启用，SPI4/5/6 未启用）。

### 问题

`boards/arm/stm32h7/stm32h750b-dk/include/board.h:122-128`：

```c
#define STM32_PLLCFG_PLL2Q       4
#define STM32_PLLCFG_PLL2R       4

#define STM32_VCO2_FREQUENCY     ((STM32_HSE_FREQUENCY / 5) * 160)
#define STM32_PLL2P_FREQUENCY    (STM32_VCO2_FREQUENCY / 2)
#define STM32_PLL2Q_FREQUENCY
#define STM32_PLL2R_FREQUENCY
```

三个问题：

1. `STM32_PLLCFG_PLL2Q` 是裸 `4`，不是 `RCC_PLL2DIVR_Q2(4)`。对比同文件 PLL1 的写法（`RCC_PLL1DIVR_Q1(4)`）可以看出这是遗漏。`stm32h7x3xx_rcc.c:836` 会把它直接或运算进 `PLL2DIVR` 寄存器，若 `RCC_PLL2DIVR_Q2(n)` 不是简单移位，这个值会写错。
2. `STM32_PLL2Q_FREQUENCY` 与 `STM32_PLL2R_FREQUENCY` 定义为空。`arch/arm/src/stm32h7/stm32_spi.c:199,212` 在 SPI4/5/6 启用时会用 `STM32_PLL2Q_FREQUENCY` 计算内核时钟，空定义导致编译失败。
3. `STM32_PLLCFG_PLL2CFG` 只置了 `RCC_PLLCFGR_DIVP2EN`，没有 `DIVQ2EN`/`DIVR2EN`。若 SAI 需要 PLL2Q 或 PLL2R 作核时钟，必须先打开对应的使能位。

### 对 SAI 的影响

H7 的 SAI 核时钟来自 `RCC_D2CCIP1R_SAI23SEL`（SAI2/3）与 `RCC_D3CCIPR_SAI4ASEL`（SAI4），可选 PLL1/2/3 或外部时钟（`stm32h7x3xx_rcc.h:519-525, 649-654`）。现有 `board.h` 已启用 PLL2 且未分配用途，是 SAI 核时钟的自然选择。

需要注意 SAI 的 MCLK 要覆盖采样率分频。按 ST BSP 对 SAI4 的算法（`MX_SAI4_ClockConfig`，`bsp/stm32h750b_discovery_audio.c:496-528`），48k 族用 PLL2N=344、PLL2P=7（PLL2M=25），44.1k 族用 N=429、P=38。这是 BSP 在 25 MHz HSE 下算出的参数，可以直接作为 BVO-03a 的参考，但要重新验证与本板当前 PLL2 设置（M=5、N=160）是否兼容。两者 M 值不同（5 对 25），说明 BSP 的方案需要改 `board.h` 才能用。

### 结论

时钟需要在 BVO-03a 里处理，范围包括：修正 `board.h` 的 PLL2 Q/R 定义、按选定采样率重算 PLL2 参数、选择 SAI23SEL/SAI4ASEL 源。这会改动板级头文件，属于 nuttx 仓改动，要在功能分支提交。

## 9. 接口参数合同

本节固化给 BVO-02 至 BVO-05 使用。凡标为待定的，由对应任务实测后回填本文。

| 项目 | 取值或规则 | 依据 | 最终确认 |
|---|---|---|---|
| 采集设备节点 | `/dev/audio/pcm0c` | `packages/ai_agent/include/agent_config.h:374`；`audio_get_path` 默认加 `/dev/audio` 前缀（`nuttx/audio/audio.c:1793-1800`） | BVO-03 |
| 播放设备节点 | `/dev/audio/pcm0p` | `agent_config.h:377` | BVO-04 |
| 采样格式 | 16 kHz、16 bit、小端、单声道 | `voice_asr.h` 的 PCM 合同；BSP 支持 8k-96k | BVO-03/04 |
| 单次录音上限 | 15 秒，代码强制截断 | 执行方案第 2 节 | BVO-02/03 |
| 原始音频上限 | 480,000 B | 16000 × 2 × 15 | BVO-03 |
| 录音缓冲位置 | AXI SRAM 或 SDRAM；**禁止 DTCM/ITCM** | `stm32_dma.c` 的 `dmacapable` 拒绝 DTCM/ITCM | BVO-03 |
| 路线 A 额外约束 | 缓冲必须在 SRAM4，且分段搬运 | BDMA 仅可达 D3 与 SRAM4 | BVO-03 |
| DMA 缓冲对齐 | 按 `ARMV7M_DCACHE_LINESIZE` 对齐 | 驱动自身要求。内核对齐检查在写透模式下被条件编译掉（`stm32_dma.c:2107-2125` 的 `#if` 排除 `_WRITETHROUGH`），不能靠它兜底 | BVO-03 |
| cache 维护 | RX 完成后 `up_invalidate_dcache`；TX 前确保数据写出 | `stm32_dma.h` 回调注释 | BVO-03/04 |
| 播放音频格式 | WAV（`AUDIO_FMT_PCM`）；不使用 MP3 | `nuttx/audio/pcm_decode.c` 只解 WAV；`apps/audioutils/libmad/Makefile` 需构建期下载源码 | BVO-06/11 |
| 录放互斥 | 同一时刻只有一个音频会话，先停止再切换 | 执行方案第 2 节；SAI2 时钟共享 | BVO-05 |
| 播放音量初值 | 保守初值，实测后固化（BSP 默认 250/1000） | BSP `WM8994_INITVOLUME` | BVO-04 |
| 按键事件源 | 优先 `INPUT_BUTTONS` + `board_button_irq`；若成本过高退到 board 内 `stm32_gpiosetevent` 直接投递 | `drivers/input/button_lower.c:212-217` | BVO-02 |
| 按键极性 | 按下为高（PULLDOWN + IT_RISING） | BSP `BSP_PB_Init` | BVO-02（M-05 复核） |
| 采集 DMA 通道 | 走 DMA1/DMA2（路线 B）；路线 A 走 BDMA | `stm32_dma.c` 的域限制 | BVO-03a |
| SAI 核时钟源 | PLL2，参数在 BVO-03a 重算 | 第 8 节 | BVO-03a |

待实测才能定：可用采样率档位、WM8994 输入增益、MICBIAS1 是否需软件使能、绿色插座对应输出通道、播放音量安全初值、录音缓冲实际大小。

## 10. 待实测清单

完整清单见 [pre-measurement-checklist.md](pre-measurement-checklist.md)。摘要：

| 编号 | 事项 | 阻断性 |
|---|---|---|
| M-01 | 焊桥装配决定麦克风走 SAI4 还是 WM8994 | **是**，决定采集路线 |
| M-02 | 绿色插座由哪一路模拟输出驱动 | 是，决定播放代码 |
| M-03 | 两个 2x1 跳线排针的用途 | 否 |
| M-04 | 蓝色插座接 WM8994 哪个输入 | 否 |
| M-05 | PC13 按下电平 | 否 |
| M-06 | MICBIAS1 是否需软件使能 | 否 |
| M-07 | 板卡实物版本（B-01/B-02） | 否 |

## 11. 未决风险与不做什么

风险：

1. M-01 与 M-02 若指向路线 A 与 LINEOUT 路径，软件工作量会明显增加，BVO-03/04 的工期要重新估算。
2. `board.h` 的 PLL2 定义缺陷会同时影响 SAI 与未来任何 SPI4/5/6 用途，修正时要一并验证不影响 LCD 与网络。
3. `wm8994.c` 的输入路径是整段新写，没有现成参考实现可对比，只能靠 WM8994 手册与 BSP 配置核对。
4. BVO-03a 是新增任务，执行方案的依赖关系需要相应更新；本轮只在 `maintenance-todo.md` 记录，不改执行方案原文。

本轮不做：

- 不写任何驱动代码，不修改 nuttx、apps、packages 的任何文件。
- 不做 BVO-02 至 BVO-05 的实现。
- 不改 `scripts/configs/velaguard-lvgl.defconfig`。
- 不修改 `AGENTS.md`、ADR 0009、执行方案。
- 不动 `maintenance-todo.md` 里 `OUT-01` 等既有表述。
- 不声称任何编译、烧录或板端运行结果。

## 12. 版本记录

核对时间：2026-10-21，Asia/Shanghai。

| 仓库 | 分支 | HEAD | 未提交改动 |
|---|---|---|---|
| `contest2026_004_TeamFalcons` | `velaguard/maintenance-stability` | `6b9ec87` | 有（`AGENTS.md`、推进方案删除、维护证据日志、AI 日志目录、`connect_info`） |
| `../nuttx` | `velaguard/eth-mii-stm32h750b-dk` | `1b5e71d2f87` | 有（`velaguard-lvgl/defconfig`、`arch/arm/src/armv7-m/vg_diag.h`） |
| `../apps` | `velaguard/netinit-esp8266` | `0ea954b79` | 无 |
| `../packages` | `velaguard/agent-hmi-stack-heap-fix` | `9276188` | 无 |

资料包路径：`/mnt/f/Project/Embeded/H750B-DK/BOARD INFO/H750B-DK`（WSL 挂载）。ST BSP 版本 v3.3.4，Board ID MB1381B。

本轮未构建、未烧录、未在板端执行任何命令；板端基线数字引自 2026-10-18 的既有记录，不代表当前固件。
