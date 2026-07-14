# RJ45 First Network Manager With ESP-01 Fallback

Status: incomplete

## Parent

.scratch/velaguard-independent-edge-ai-gateway/PRD.md

## What to build

Add a network manager that exposes one active network path to VelaGuard. RJ45 Ethernet is primary, ESP-01 Wi-Fi is backup, reconnect uses bounded exponential backoff, and network failures must not affect Modbus acquisition, local alarms, local logs, or UI operation.

Hardware contract for ESP-01 (authoritative): `docs/velaguard-expansion-board.md`.

| Item | Contract |
|------|----------|
| UART | **USART2** on STMod+: MCU TX `PD5` (P1-2), RX `PD6` (P1-3) — exclusive; not shared with RS485 or VCP |
| Control | RST `PH10` (P1-12), EN/CH_PD `PA3` (P1-14) |
| Power | Independent **3V3 LDO** from board 5V, peak ≥500 mA; not a weak shared 3V3 rail only |
| DK solder bridges | Target UART mode: SB16/SB12 ON, SB13/SB11 OFF, SB21 OFF (factory SPI defaults in doc §5) |
| Forbidden | D0/D1 VCP; I2C4; STMod P1-11 PH12 (FDCAN1_TX) |

## Acceptance criteria

- [ ] Network state is shown in the LVGL UI with active bearer, IP state, and reconnect status.
- [ ] RJ45 is selected when available and healthy.
- [ ] ESP-01 is selected only when RJ45 is unavailable or unhealthy.
- [ ] VelaGuard does not send duplicate cloud traffic over both links at once.
- [ ] RJ45 failback waits for a configured stability window.
- [ ] Reconnect attempts use exponential backoff with a configured maximum and jitter.
- [ ] ESP-01 driver uses a **dedicated USART2** path (STMod contract) and is non-blocking for acquisition/alarm tasks.
- [ ] ESP-01 has a failure counter and supports **GPIO hardware reset** (RST and/or EN) after repeated failure.
- [ ] ESP-01 power/reset requirements are documented against the expansion board LDO and pin map.
- [ ] Link loss writes structured events and updates UI state.
- [ ] With both links down, the Local Safety Loop continues normally.

## Hardware notes

- DK must be reworked to STMod USART2 bridges before hardware bring-up; rollback table in expansion-board doc.
- Soft-UART or Arduino bit-bang for ESP is out of scope for the product path.

## Blocked by

- .scratch/velaguard-independent-edge-ai-gateway/issues/01-bootable-velaguard-skeleton.md
