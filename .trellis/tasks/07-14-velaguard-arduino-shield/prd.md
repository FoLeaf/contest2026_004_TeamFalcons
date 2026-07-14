# Plan VelaGuard expansion board for H750B-DK (Arduino + STMod+)

## Goal

基于 STM32H750B-DK **全部扩展扇出**（Arduino Uno CN2/CN3/CN6/CN7 + STMod+ P1），规划 **单板一体** VelaGuard 扩展板（Arduino 母座 + STMod 插针/软排线），在不破坏板载 LCD/Touch/Audio/Ethernet/ST-LINK VCP 的前提下，提供：

1. **RS485 / Modbus RTU** 工业采集
2. **ESP-01 Wi-Fi** 备用网络
3. 电源、指示、可选 DO/告警输出
4. 与 openvela 固件合同对齐的引脚与设备路径

## Background / Confirmed Facts

### Product need

- RS485 Modbus master、RJ45 主网 + ESP-01 备用、本地安全闭环离线可用。
- UART 资源：**RS485 / ESP-01 / debug 三者分离**。
- 已有固件设计锁定 RS485 = **UART7**：TX `PB4/D10`，RX `PA8/D5`，DIR `PK1/D4`；`/dev/rs485`；**禁止占用 D0/D1（USART3 = ST-LINK VCP）**。

### Hardware truth sources

- `STM32H750B-DK_外设与引脚分配.md` + `STM32H750B-DK_pinmux.csv`
- 原理图 MB1381B；STMod fanout 参考 MB1280
- 主板已有：Ethernet、FDCAN1/2 收发器、I2C4（Touch+Codec）、SAI Audio、ST-LINK VCP

---

## Full fanout inventory

### A. Arduino connectors — usability

| Pin | Conn | MCU | Default AF | Usability | VelaGuard plan |
|-----|------|-----|------------|-----------|----------------|
| D0 | CN2-1 | PB11 | USART3_RX | **FORBIDDEN** (ST-LINK VCP) | NC / do not load |
| D1 | CN2-2 | PB10 | USART3_TX | **FORBIDDEN** (ST-LINK VCP) | NC / do not load |
| D2 | CN2-3 | PG3 | GPIO | Free | **LED_RS485_ACT** |
| D3 | CN2-4 | PA6 | TIM13_CH1 | Free PWM | **LED_STATUS** |
| D4 | CN2-5 | PK1 | GPIO | Free | **RS485 DIR** (locked) |
| D5 | CN2-6 | PA8 | UART7_RX / TIM1 | Free (use UART7) | **RS485 RX** (locked) |
| D6 | CN2-7 | PE6 | TIM15_CH2 | Free | **DO1 低边开关** (D4/D5) |
| D7 | CN2-8 | PI8 | GPIO | Free | 备用 / ESP_EN 备选 |
| D8 | CN6-1 | PE3 | GPIO | Free | 备用 |
| D9 | CN6-2 | PH15 | TIM8_CH3N | Free PWM | 备用 |
| D10 | CN6-3 | PB4 | UART7_TX / SPI2_NSS | Free (use UART7) | **RS485 TX** (locked) |
| D11 | CN6-4 | PB15 | SPI2_MOSI | Free* | SPI 保留 / 勿与 STMod SPI 同时用 |
| D12 | CN6-5 | PI2 | SPI2_MISO | Free* | SPI 保留 |
| D13 | CN6-6 | PD3 | SPI2_SCK | Free* (板载 LD8) | SPI 保留；注意 Arduino LED |
| D14/SDA | CN6-9 | PD13 | I2C4_SDA | **Shared** Touch/Codec | **禁止外挂主设备** |
| D15/SCL | CN6-10 | PD12 | I2C4_SCL | **Shared** Touch/Codec | **禁止外挂主设备** |
| A0 | CN7-1 | PC0 | ADC | Free | 可选模拟传感 / 预留 |
| A1 | CN7-2 | PF8 | ADC3 | Free | 预留 |
| A2 | CN7-3 | PA0_C | ADC | Free analog | 预留 |
| A3 | CN7-4 | PA1_C | ADC | Free analog | 预留 |
| A4 | CN7-5 | PC2_C | ADC | Free; SB34→I2C4 | 保持 ADC，勿切 I2C4 |
| A5 | CN7-6 | PC3_C | ADC | Free; SB36→I2C4 | 保持 ADC，勿切 I2C4 |
| 3V3 | CN3 | — | Power | OK | 数字 3V3 |
| 5V | CN3 | — | Power | OK | 继电器/外部 5V 器件 |
| GND | CN3 | — | Power | OK | 共地 |
| IOREF | CN3 | — | 3V3 参考 | OK | 电平参考 |
| NRST | CN3 | — | Reset | 慎用 | 不接业务复位链 |
| VIN | CN3 | — | 外部供电 | **风险 &lt;11.5V** | **不使用** |

\* D11–D13 与 STMod SPI 选项同源；若 STMod 切到 USART2 模式（见下），Arduino SPI 仍可用。

### B. STMod+ P1 — usability（含焊桥）

ST 通过 **solder bridge** 在 SPI 与 USART2 间二选一。VelaGuard **推荐 USART2 模式**（给 ESP-01）。

| P1 pin | SPI mode (default-ish) | UART mode (recommended) | MCU | Usability | Plan |
|--------|------------------------|-------------------------|-----|-----------|------|
| 1 | NSS (SB21) PA15 **JTAG 冲突** | CTS (SB19) PA0 | PA15/PA0 | UART 模式更安全 | ESP 不用 CTS；NC 或预留 |
| 2 | MOSI (SB13) PB15 = D11 | **TX (SB16) PD5** | PD5 | **需 SB16 合、SB13 断** | **ESP-01 RXD**（MCU TX→模组 RX） |
| 3 | MISO (SB11) PI2 = D12 | **RX (SB12) PD6** | PD6 | **需 SB12 合、SB11 断** | **ESP-01 TXD**（模组 TX→MCU RX） |
| 4 | SCK (SB10) PD3 = D13 | RTS/DE (SB9) PD4 | PD4 | 可选 | 预留 / 不用 |
| 5–6 | 3V3 / GND | same | — | Power | ESP 数字地；**3V3 勿直供 ESP 峰值** |
| 7 | SCL PD12 | same | PD12 | **I2C4 共享** | 禁止 |
| 8 | MOSIs PI3 | — | PI3 | Free-ish | 备用 |
| 9 | MISOs PB14 | — | PB14 | Free-ish | 备用 |
| 10 | SDA PD13 | same | PD13 | **I2C4 共享** | 禁止 |
| 11 | INT PH12 | — | PH12 | **FDCAN1_TX 冲突** | **禁用**（保留 CAN） |
| 12 | RST PH10 | — | PH10 | Free | **ESP-01 RST** |
| 13 | ADC PA4 | — | PA4 | Free | 预留 / 电流检测 |
| 14 | PWM PA3 | — | PA3 | Free | **ESP-01 CH_PD/EN** |
| 15–16 | 5V / GND | same | — | Power | 可作 ESP LDO 输入 |
| 17 | GPIO PH1 | — | PH1 | OSC_OUT 风险 | **禁用** |
| 18 | GPIO PI11 | — | PI11 | Free | 状态 LED / 备用 |
| 19 | GPIO PH4 | — | PH4 | Free | 备用 |
| 20 | GPIO PH8 | — | PH8 | Free | 备用 |

### C. On-board resources — do not re-implement on shield

| Resource | Location | Note |
|----------|----------|------|
| Ethernet RJ45 | CN1 | 主网 |
| FDCAN1/2 | CN10/CN11 | 可选 CAN；勿抢 PH12/PH13/PB5/PB13 |
| ST-LINK VCP | USB CN14 | 调试 console |
| LCD / Touch / Audio | 板载 | 占用 I2C4、LTDC、SAI |

---

## Recommended architecture (Arduino + STMod combined)

```text
                    STM32H750B-DK
         ┌──────────────────────────────────┐
         │  LCD / Touch / Audio / ETH / CAN │
         │  ST-LINK VCP = USART3 D0/D1      │
         └───────────┬───────────┬──────────┘
                     │           │
              Arduino CN*     STMod+ P1
                     │           │
         ┌───────────▼───┐   ┌───▼──────────────┐
         │ RS485 区块     │   │ ESP-01 区块       │
         │ UART7 TX/RX/DIR│   │ USART2 TX/RX     │
         │ A/B + 120R+TVS │   │ RST/EN + 独立3V3 │
         │ LED / 可选 DO  │   │ （经焊桥切 UART） │
         └───────────────┘   └──────────────────┘
```

### Function → pin map (baseline)

| Function | Interface | Pins | Connector |
|----------|-----------|------|-----------|
| RS485 DI | UART7_TX | PB4 = D10 | Arduino |
| RS485 RO | UART7_RX | PA8 = D5 | Arduino |
| RS485 DE/RE | GPIO DIR | PK1 = D4 | Arduino |
| RS485 A/B | 端子 | — | Shield edge |
| Console | USART3 | PB10/11 = D1/D0 | **不接业务** |
| ESP UART | USART2 | PD5 TX, PD6 RX | STMod P1-2/3 (SB16/SB12) |
| ESP RST | GPIO | PH10 = P1-12 | STMod |
| ESP EN/CH_PD | GPIO | PA3 = P1-14 | STMod |
| ESP 3V3 | LDO from 5V | P1-15 5V → LDO ≥500mA | STMod |
| RS485 act LED | GPIO | PG3 = D2 | Arduino |
| Status LED | GPIO | PA6 = D3 | Arduino |
| Optional DO | GPIO+MOSFET | PE6 = D6 | Arduino |
| SPI 扩展预留 | SPI2 | D11–D13 | Arduino（二期） |
| I2C 外设 | — | — | **不做**（I2C4 共享） |

### Board solder-bridge checklist (H750B-DK 本体)

| Bridge | Required state for VelaGuard | Purpose |
|--------|------------------------------|---------|
| SB16 | **closed** | STMod P1-2 → USART2_TX PD5 |
| SB13 | **open** | 断开 SPI MOSI 到 STMod |
| SB12 | **closed** | STMod P1-3 → USART2_RX PD6 |
| SB11 | **open** | 断开 SPI MISO 到 STMod |
| SB9 / SB10 | 默认；RTS 不用可保持 SPI SCK 在 Arduino | 按需 |
| SB21 | **open** | 避免 PA15 JTAG 接到 STMod NSS |
| SB19 | open（不用 CTS） | — |
| SB34 / SB36 | **open** | A4/A5 保持 ADC，不抢 I2C4 |

> 实际焊桥默认状态以 MB1381B 原理图为准；实现前必须对照实物/原理图核对，本表为**目标配置**。

---

## Requirements

### R1 Form factor — **Decision: 方案 A 单板一体**

- **单板**：Arduino 母座对接 CN2/CN3/CN6/CN7，**同板** 用插针或短软排线对接 STMod+ P1。
- 板上分区：RS485（Arduino 侧）+ ESP-01（STMod 侧）+ 电源/LED/端子。
- 机械上必须对齐 H750B-DK 两连接器相对位置；不遮挡 LCD、RJ45、ST-LINK USB。
- 不做双板拆分；不做杜邦飞线交付形态。

### R2 RS485 (MVP)

- 3.3V 半双工收发器；A/B；可跳线 120Ω；TVS（SM712 类）。
- 引脚锁定 UART7：D10/D5/D4。
- 与 VCP 隔离；共地；Gate B 可测。

### R3 ESP-01 (同方案必含，走 STMod)

- 独占 USART2（STMod UART 模式）。
- 独立 3V3 LDO（输入取 5V），峰值 ≥500mA。
- RST + EN 可控；连续失败可硬复位。
- 天线侧远离金属/LCD 金属框。

### R4 Power / indicators / DO — **Decision D4: 完整 1 路 DO**

- 数字逻辑 3V3；ESP 专用 LDO；VIN 不用。
- LED：PWR、RS485 活动、STATUS（可用 D2/D3）。
- **第一版含 1 路 DO**：MCU `D6/PE6` → **低边 MOSFET/开漏** → 对外 OUT/GND（及可选外供 V+ 端子说明）；用途为本地告警联动/演示（灯/蜂鸣器）；**非 AI 直控**（固件策略另任务）。
- 负载与 MCU **共地**；文档规定最大电压/电流与续流/TVS 要求。

### R5 Documentation — **Decision: 方案 A 文档交付**

- 交付：全量扇出表、原理图分区说明、焊桥清单、BOM、布局约束、接线与验收（Gate A/B + ESP AT）。
- 输出：任务内 `design.md` + 仓库 `docs/velaguard-expansion-board.md`（implement 定稿）。
- **本任务不做** KiCad 原理图/PCB/Gerber（可另开任务）。

## Acceptance Criteria

- [x] Arduino + STMod 每个可用脚有：用途 / 禁用原因 / 冲突。
- [x] RS485 与现有 UART7 design 完全一致。
- [x] ESP-01 有独立 UART/电源/复位，不与 RS485/VCP/I2C4/FDCAN 冲突。
- [x] 焊桥目标配置可执行、可回滚（含 B01 出厂态）。
- [x] BOM 3.3V 逻辑兼容；ESP 供电裕量明确。
- [x] 布局原则可指导画板（无需本任务产出 Gerber）。
- [x] 文档已落入 `docs/velaguard-expansion-board.md`。
- [x] Out of scope 明确。

## Out of Scope

- 应用固件 Modbus/MQTT 实现（其他 issue）。
- 隔离 RS485 / EMC 认证。
- 重做板载 FDCAN/Ethernet。
- 占用 D0/D1 或 I2C4 外挂主设备。
- 主板 PCB 改版。
- **本任务内的 KiCad/PCB/Gerber**（交付深度 D2）。

## Decisions

| ID | Decision | Date |
|----|----------|------|
| D1 | 物理形态 = **方案 A 单板一体**（Arduino + STMod 同板） | 2026-07-14 |
| D2 | 交付深度 = **文档方案**（扇出/电气/BOM/布局/验收；无 KiCad） | 2026-07-14 |
| D3 | 主板焊桥 = **接受改板** 启用 STMod USART2（目标：SB16/SB12 合，SB13/SB11/SB21 断；可回滚） | 2026-07-14 |
| D4 | 第一版含 **完整 1 路 DO**（D6/PE6），用于告警联动/演示，非 AI 直控 | 2026-07-14 |
| D5 | DO 形态 = **低边 MOSFET/开漏**（非干接点继电器） | 2026-07-14 |

## Open Questions

（规划决策已收敛；实现前仅需对照 MB1381B 原理图核对焊桥默认态与连接器机械尺寸。）
