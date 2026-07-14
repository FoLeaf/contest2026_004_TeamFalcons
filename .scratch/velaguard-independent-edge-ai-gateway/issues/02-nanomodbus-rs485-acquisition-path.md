# nanoMODBUS RS485 Acquisition Path

Status: incomplete

## Parent

.scratch/velaguard-independent-edge-ai-gateway/PRD.md

## What to build

Add the first end-to-end Modbus RTU acquisition path using nanoMODBUS over an openvela/NuttX UART transport. VelaGuard should read one to three registers from one Modbus slave or simulator, convert the values, and show the live readings on the home screen and device detail view.

Hardware contract (authoritative): `docs/velaguard-expansion-board.md` and Trellis task `07-13-resolve-usart3-rs485-vcp-conflict` (UART7 foundation).

| Item | Contract |
|------|----------|
| Port | Prefer `/dev/rs485` (softlink to UART7 tty); do not hard-code only `/dev/ttyS1` in product code |
| Pins | TX `PB4` / Arduino **D10**, RX `PA8` / **D5**, DIR `PK1` / **D4** (DE+/RE tied, high = TX) |
| Forbidden | Arduino **D0/D1** (`USART3` = ST-LINK VCP console) — never use for Modbus |
| Level | 3.3V transceiver on expansion board; A/B + optional 120Ω + TVS |
| Demo baud | 9600 8N1 default (configurable later) |

## Acceptance criteria

- [ ] VelaGuard can configure one Modbus RTU slave with serial settings, slave address, function code 03 or 04, register address, data type, scale, offset, and unit.
- [ ] nanoMODBUS is used through a platform transport adapter rather than being rewritten.
- [ ] The transport adapter opens and configures the openvela/NuttX serial device and implements nanoMODBUS read/write behavior with timeouts.
- [ ] Product code opens **`/dev/rs485`** (or documented alias) for Modbus, not USART3/VCP.
- [ ] RS485 DE/RE uses UART7 DIR on **D4/PK1** (driver RS485 or GPIO), idle = receive.
- [ ] Pin map matches expansion board: **D10/D5/D4**; docs state D0/D1 are console-only.
- [ ] The board reads one to three registers every one to two seconds from a real device or Modbus simulator.
- [ ] Live readings are shown in the LVGL UI with value, unit, last update, and quality.
- [ ] A test-read action reports success or a clear failure reason.
- [ ] Modbus acquisition failure does not crash or block the UI task.

## Hardware notes

- Bench may use USB-RS485 peer before the full shield is fabricated; electrical path must still be UART7 + DIR, not VCP.
- Expansion board BOM and termination: see `docs/velaguard-expansion-board.md` §3.1 / §7.
- **Without hardware:** use Kconfig `VG_ACQ_BACKEND_MOCK` (`app/velaguard_app` `vg_acq_*`) for UI/demo; replace with `VG_ACQ_BACKEND_UART` when implementing this issue. Do not remove the mock seam.

## Blocked by

- .scratch/velaguard-independent-edge-ai-gateway/issues/01-bootable-velaguard-skeleton.md
- UART7 `/dev/rs485` board enablement (Trellis `07-13-resolve-usart3-rs485-vcp-conflict` or equivalent patch) before Gate B on real bus
