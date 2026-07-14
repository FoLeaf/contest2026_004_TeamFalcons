# Design: VelaGuard H750B-DK expansion board (Arduino + STMod single PCB)

## Architecture and Boundaries

```text
                    STM32H750B-DK (MB1381B)
         ┌─────────────────────────────────────────────┐
         │ LCD/Touch/Audio | ETH | FDCAN | ST-LINK VCP │
         │ USART3 = D0/D1  (console only)              │
         └───────────┬───────────────────┬─────────────┘
                     │ Arduino           │ STMod+ P1
                     │ CN2/3/6/7         │ (USART2 mode)
         ┌───────────▼──────────┐   ┌────▼─────────────────┐
         │ RS485 + LED + DO     │   │ ESP-01 + LDO + RST/EN│
         │ UART7 TX/RX/DIR      │   │ USART2 TX/RX         │
         └──────────────────────┘   └──────────────────────┘
              单板一体 (Decision D1)
```

| Layer | Owner | This task |
|-------|-------|-----------|
| 扇出/引脚/焊桥合同 | `docs/velaguard-expansion-board.md` + 本 design | Yes |
| 原理图分区 + BOM + 布局约束 | 同上 | Yes |
| 验收清单 (电气/机械) | 同上 | Yes |
| KiCad / Gerber | — | **No** (D2) |
| 固件 UART7 RS485 | task `07-13-...` / later issues | No (consume contract) |
| 固件 ESP-01 / DO 驱动 | later issues | No (document pin contract only) |

## Electrical contracts

### 1. RS485 (Arduino / UART7) — locked with firmware

| Signal | MCU | Arduino | Transceiver | Notes |
|--------|-----|---------|-------------|-------|
| TX | PB4 | D10 | DI | UART7_TX |
| RX | PA8 | D5 | RO | UART7_RX |
| DIR | PK1 | D4 | DE + /RE tied | high = TX; idle low = RX |
| VCC | 3V3 | CN3 3V3 | VCC | 非隔离 |
| A/B | — | 端子 | A/B | SM712 类 TVS；120Ω 可跳线 |
| GND | GND | CN3 | GND | 与对端共地（竞赛级） |

- **禁止** 使用 D0/D1（USART3 / ST-LINK VCP）。
- 建议收发器：SP3485 / MAX3485 / THVD1451 等 **3.3V** 器件。
- 固件路径：`/dev/rs485`（见 `07-13` design）；默认 9600 8N1 可演示。

### 2. ESP-01 (STMod+ / USART2)

| Signal | MCU | STMod | ESP-01 | Notes |
|--------|-----|-------|--------|-------|
| MCU TX | PD5 | P1-2 (SB16) | RXD | 经分压或直接 3.3V 逻辑 |
| MCU RX | PD6 | P1-3 (SB12) | TXD | 3.3V |
| RST | PH10 | P1-12 | RST | 低有效；MCU 推挽或开漏+上拉 |
| EN/CH_PD | PA3 | P1-14 | CH_PD | 高使能；上拉 + MCU 可控 |
| 3V3 | LDO out | — | VCC | **独立 LDO**，输入取 P1-15 **5V**，Iout ≥500mA 峰值 |
| GND | GND | P1-6/16 | GND | 单点汇合 |

**主板焊桥 (D3)** — B01 出厂来自 `MB1381-B01_PickandPlace.txt`（SB_ON=导通）：

| Bridge | Factory B01 | Target | Effect |
|--------|-------------|--------|--------|
| SB16 | OFF | **ON** | P1-2 → USART2_TX PD5 |
| SB13 | ON | **OFF** | 断 SPI MOSI→STMod |
| SB12 | OFF | **ON** | P1-3 → USART2_RX PD6 |
| SB11 | ON | **OFF** | 断 SPI MISO→STMod |
| SB10 | ON | ON | 可保持 Arduino D13 SPI SCK |
| SB9 | OFF | OFF | 不用 RTS |
| SB21 | ON | **OFF** | 避 PA15 JTAG→STMod NSS |
| SB19 | OFF | OFF | 不用 CTS |
| SB34/SB36 | OFF | OFF | A4/A5 不抢 I2C4 |

回滚：恢复 Factory 列；扩展板 ESP 区可不贴装。权威表见 `docs/velaguard-expansion-board.md` §5。


### 3. DO1 低边开关 (D5)

| Item | Spec |
|------|------|
| 控制脚 | PE6 = Arduino D6，高 = 导通（推荐） |
| 拓扑 | N-MOSFET 低边；栅极串阻 + 下拉；漏极 = OUT 端子 |
| 负载 | 外部 V+（建议 ≤12V）经负载到 OUT，回流到板 GND |
| 保护 | 感性负载续流二极管；OUT 对地 TVS 可选；限流/保险按 0.5–1A 演示级 |
| 共地 | **必须** 与 H750 共地；文档醒目标注 |
| 非目标 | 干接点、强电、隔离 |

### 4. Indicators

| LED | Drive | Meaning |
|-----|-------|---------|
| PWR | 3V3 经电阻 | 扩展板得电 |
| RS485_ACT | D2/PG3 | 发送或总线活动（固件可选） |
| STATUS | D3/PA6 | 通用状态（固件可选） |

### 5. Power tree

```text
H750 5V (CN3 / STMod P1-15)
  ├─→ ESP LDO → 3V3_ESP → ESP-01 only
  └─→ (可选) DO 负载外部供电说明，不强制板载 12V

H750 3V3 (CN3)
  └─→ RS485 收发器、LED、逻辑

VIN (CN3) → 不使用
```

## Forbidden / conflict map

| Resource | Why forbidden on shield |
|----------|-------------------------|
| D0/D1 | ST-LINK VCP / console |
| D14/D15, STMod SCL/SDA | I2C4 shared with Touch+Codec |
| STMod P1-11 PH12 | FDCAN1_TX |
| STMod P1-17 PH1 | OSC_OUT risk |
| Re-implement ETH/FDCAN | 已在主板 |

## Mechanical / layout principles

1. **Arduino 母座** 对齐 CN2/CN3/CN6/CN7；堆叠高度避开 LCD 边框与按键。
2. **STMod 连接**：同板插针或 ≤10cm 软排线到 P1；应变消除。
3. **RS485 端子** 靠板外沿；A/B 丝印；120Ω 跳线可及。
4. **ESP-01** 天线侧无金属正对 LCD 铁框；LDO 靠近模组，宽地、去耦。
5. **DO 端子** 与 RS485 分区；丝印 `DO1 OUT` / `GND` / 额定。
6. 测试点：UART7 TX/RX/DIR、USART2 TX/RX、3V3_ESP、DIR。

## BOM (baseline, 可替换同规格)

| Ref | Function | Example P/N class |
|-----|----------|-------------------|
| U_RS | 3.3V RS485 | SP3485EN / MAX3485 / THVD1451 |
| TVS | A/B 保护 | SM712 或 2×双向 TVS |
| Rterm | 120Ω | 跳线接入 |
| U_LDO | ESP 3V3 | AMS1117-3.3 或更好 LDO ≥800mA |
| U_ESP | Wi-Fi | ESP-01 / ESP-01S 座 |
| Q_DO | N-MOSFET | 2N7002 / AO3400 级（按电流选型） |
| D_fly | 续流 | SS14 / 1N4148（按负载） |
| J_AB | 端子 | 3.5/5.08 螺钉端子 2P |
| J_DO | 端子 | 2P OUT+GND |
| HDR | Arduino + STMod | 母座 + STMod 兼容插针 |
| LEDx | 指示 | 0805 + 限流电阻 |

## Documentation deliverable shape

单文件权威文档：`docs/velaguard-expansion-board.md`，章节：

1. 概述与架构  
2. 全量扇出表（Arduino + STMod）  
3. 功能引脚合同（RS485 / ESP / DO / LED）  
4. 焊桥清单与回滚  
5. 电源树与 BOM  
6. 布局与机械  
7. 验收：Gate 电气 + 与固件 Gate A/B 的接口  
8. Out of scope / 二期  

任务 `prd.md` 保留需求；本 `design.md` 保留技术合同；`implement.md` 为写作与核对清单。

## Trade-offs

| Choice | Why | Rejected |
|--------|-----|----------|
| 单板 Arduino+STMod | 一体交付、少飞线 | 双板 / 纯杜邦 |
| ESP 走 STMod USART2 | 硬串口、与 UART7 分离 | Arduino bit-bang |
| 改焊桥 | 稳定 USART2 | 零改板软串 |
| 低边 MOSFET DO | 简单够演示 | 干接点继电器 |
| 文档-only 本任务 | 先钉合同 | 本任务出 Gerber |

## Compatibility and rollback

- **Preserve**: VCP console、LCD/Touch、ETH、FDCAN 连接器可用、UART7 RS485 固件合同。
- **Rollback hardware**: 拔扩展板；焊桥恢复出厂；D0/D1 始终无负载。
- **Risk**: 焊桥默认态与文档不一致 → 实施前必须实物核对；STMod 与 Arduino 机械干涉 → 布局阶段量板。

## Out of design scope

- KiCad 工程与投板文件  
- nanoMODBUS / network_manager / DO 应用逻辑  
- 隔离 RS485、EMC 认证、强电继电器  
