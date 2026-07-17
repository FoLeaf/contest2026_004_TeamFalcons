# Design: UART7 RS485 foundation (USART3 VCP preserved)

## Architecture and Boundaries

```
PC COM7 (ST-LINK VCP) ── R184/R188 ── USART3 PB10/PB11 ── /dev/ttyS0, console, NSH, syslog
PC USB-RS485 COM*     ── A/B + SM712 + 120Ω ── 3.3V transceiver ── UART7 PB4/PA8 + PK1 DIR ── /dev/ttyS1 + /dev/rs485
```

| Layer | Owner | This task |
|-------|-------|-----------|
| Pin map + `GPIO_UART7_RS485_DIR` | NuttX board `board.h` (via contest patch) | Yes |
| `/dev/rs485` stable alias | NuttX board `stm32_bringup.c` (via contest patch) | Yes |
| UART7 + RS485 Kconfig | Build script `kconfig-tweak` after configure | Yes |
| Bench tools `serialrx` / `serialblaster` | Test-mode only (`VelaGuardMode=test`) | Yes |
| Windows peer script | Contest `scripts/` | Yes |
| Wiring / Gate A/B docs | `docs/velaguard-rs485-uart7.md` + README link | Yes |
| Modbus / nanoMODBUS / sensors | Later issues | No |

Contest code stays under `contest2026_004_TeamFalcons/`. Upstream NuttX changes are applied only through an **idempotent, separate** patch (same pattern as QSPI), never by hand-editing the live tree as the durable source of truth.

## Pin and Electrical Contract

| Signal | MCU pin | Arduino | Transceiver | Notes |
|--------|---------|---------|-------------|-------|
| UART7 TX | PB4 | D10 | DI | Already in upstream `board.h` as `GPIO_UART7_TX` |
| UART7 RX | PA8 | D5 | RO | Already in upstream `board.h` as `GPIO_UART7_RX` |
| DIR | PK1 | D4 | DE + /RE tied | New `GPIO_UART7_RS485_DIR`; init **low** (receive) |
| VCC / GND | 3.3 V / GND | — | VCC / GND | Non-isolated first BOM; common ground with USB-RS485 peer |
| A / B | — | — | A / B | SM712 (or equiv.) on bus; 120 Ω at **both** ends |

- USART3 PB10/PB11, R184, R188: **untouched**; remain ST-LINK VCP console @ 115200 8N1.
- PK1 is free relative to board LED backlight (PK0 only). PB4 is NJTRST; project uses SWD on PA13/PA14 only.
- Polarity: `CONFIG_UART7_RS485_DIR_POLARITY=1` (high = TX enable), matching DE+/RE wiring.

## Device Path Contract

With only `CONFIG_STM32H7_USART3` + `CONFIG_STM32H7_UART7` and reordering enabled (default):

1. Console USART3 → `/dev/console` and `/dev/ttyS0`
2. Next configured non-console UART in `g_uart_devs` order → `/dev/ttyS1` = UART7

**Public VelaGuard contract:** `/dev/rs485` → `/dev/ttyS1` (UART7).

Implementation:

1. Enable `CONFIG_FS_LINKS` (required for pseudo softlinks; currently off in production config).
2. In board bringup, after serial is up, create softlink `/dev/rs485` → `/dev/ttyS1` via `symlink()` (available when `CONFIG_FS_LINKS=y`).
3. **Build-time guard:** static harness (or post-build check) asserts that with the intended UART enable set, the only non-console UART is UART7, so `/dev/ttyS1` cannot silently become another peripheral. Document that enabling any additional `STM32H7_U[S]ARTn` before UART7 renumbers tty nodes and **must** update the link target + guard.

Do not hard-code product Modbus code to `/dev/ttyS1`; always use `/dev/rs485`.

## Config Contract

### Always (production + test)

| Symbol | Value |
|--------|-------|
| `CONFIG_STM32H7_USART3` | y (unchanged) |
| `CONFIG_USART3_SERIAL_CONSOLE` | y |
| `CONFIG_USART3_BAUD` | 115200 |
| `CONFIG_STM32H7_UART7` | y |
| `CONFIG_UART7_BAUD` | 9600 |
| `CONFIG_UART7_BITS` / `PARITY` / `2STOP` | 8 / 0 / 0 |
| `CONFIG_UART7_RS485` | y |
| `CONFIG_UART7_RS485_DIR_POLARITY` | 1 |
| `CONFIG_UART7_RXDMA` / `TXDMA` | **disabled** (driver rejects RS485+DMA) |
| `CONFIG_FS_LINKS` | y |

Applied via `kconfig-tweak` in `scripts/windows_build_openvela.ps1` after `configure.sh`, same place as QSPI/VelaGuard tweaks. Do not fork a second board defconfig unless necessary.

### Test mode only (`VelaGuardMode=test`, i.e. `productModeFlag=0`)

| Symbol | Value |
|--------|-------|
| `CONFIG_EXAMPLES_SERIALRX` | y |
| `CONFIG_EXAMPLES_SERIALRX_DEVPATH` | `/dev/rs485` |
| `CONFIG_EXAMPLES_SERIALBLASTER` | y |
| `CONFIG_EXAMPLES_SERIALBLASTER_DEVPATH` | `/dev/rs485` |

Production mode must leave both examples **disabled**.

### Payload contract (26 bytes)

`serialblaster` default alphabet is already `abcdefghijklmnopqrstuvwxyz` (26 bytes). Peer script and docs use the **same** fixed sequence both directions; reject wrong length or content.

## Patch Layout (separate from QSPI)

| Asset | Role |
|-------|------|
| `scripts/openvela-uart7-rs485-stm32h750b-dk.patch` | Durable source: `board.h` DIR GPIO + `stm32_bringup.c` `/dev/rs485` link |
| `scripts/apply-openvela-uart7-rs485-patch.sh` | Idempotent apply (reverse-check → already applied; else check → apply), mirror QSPI script |
| `scripts/apply-openvela-qspi-patch.sh` | **Unchanged** detection/apply; must still succeed after UART7 patch |

Build script order: QSPI apply → UART7 RS485 apply → configure → kconfig-tweak → build.

Patch scope is minimal: only `stm32h750b-dk` board files needed for DIR + symlink. Prefer not touching arch serial driver (already supports UART7 RS485).

## Windows Peer Script

- Path: `scripts/windows_rs485_peer.ps1` (name fixed in implement checklist).
- Inputs: COM port, direction (`pc-to-board` / `board-to-pc`), optional baud (default 9600).
- Behavior: open serial 9600 8N1; send or expect the 26-byte alphabet; timeout and content mismatch → non-zero exit.
- Does not flash firmware; assumes test image already on board and operator runs matching NSH `serialrx` / `serialblaster` on `/dev/rs485`.

## Documentation

- Authoritative: `docs/velaguard-rs485-uart7.md` — pin map, BOM, common ground, SM712, termination, A/B swap diagnosis, Gate A software checklist, Gate B hardware checklist, device-path contract, test commands.
- README: one short link under docs section; no full checklist duplication.

## Trade-offs

| Choice | Why | Rejected alternative |
|--------|-----|----------------------|
| Softlink `/dev/rs485` + `CONFIG_FS_LINKS` | Stable product path; matches PRD “pseudo-filesystem link” | Hard-code `/dev/ttyS1` in app (breaks if UART set grows) |
| Separate patch vs QSPI | Independent reverse-check; avoids combined partial-apply false positives | Single mega-patch |
| kconfig-tweak in existing build script | Matches VelaGuard mode pattern; no second defconfig tree | Permanent defconfig fork in upstream board |
| Gate A before Gate B | Unblocks software while parts ship | Blocking whole task on BOM logistics |
| TVS-only, non-isolated | Contest-demo tier | Isolated transceiver (later pin-compatible upgrade) |

## Compatibility and Rollback

- **Preserve:** QSPI boot, display, touch, storage, COM7 console, attach-only SWD.
- **Rollback software:** stop applying UART7 patch; remove UART7 kconfig-tweaks; delete peer script/docs if needed. QSPI path independent.
- **Rollback hardware:** leave USART3/VCP alone; disconnect UART7 transceiver only.
- **Risk:** enabling extra UARTs renumbers `ttyS*`; mitigate with build guard + docs.

## Out of Design Scope

- nanoMODBUS, sensor maps, multi-drop, isolation, EMC certification.
- Moving console off ST-LINK VCP or removing R184/R188.
- Claiming Gate B electrical success without purchased hardware.
