# Implement: UART7 RS485 foundation

## Ordered Checklist

### 1. NuttX board patch (durable source in contest repo)

- [ ] Author `scripts/openvela-uart7-rs485-stm32h750b-dk.patch` against openvela `nuttx` tree:
  - `boards/arm/stm32h7/stm32h750b-dk/include/board.h`
    - Add `GPIO_UART7_RS485_DIR` on **PK1**:
      `GPIO_OUTPUT|GPIO_PUSHPULL|GPIO_SPEED_100MHz|GPIO_OUTPUT_CLEAR|GPIO_PORTK|GPIO_PIN1`
    - Keep existing `GPIO_UART7_TX` (PB4) / `GPIO_UART7_RX` (PA8); leave USART3 pins unchanged.
  - `boards/arm/stm32h7/stm32h750b-dk/src/stm32_bringup.c`
    - After normal bringup success path, if UART7 RS485 is enabled (or always when links available): create softlink `/dev/rs485` → `/dev/ttyS1` via `symlink()`.
    - Log failure with `syslog`; do not abort entire bringup on link failure if that would brick boot—prefer warn + continue unless link is trivial.
- [ ] Author `scripts/apply-openvela-uart7-rs485-patch.sh` mirroring QSPI apply:
  - reverse `--check` → “already applied” exit 0
  - else `apply --check` then `apply`
  - clear error if conflict
- [ ] Wire apply into `scripts/windows_build_openvela.ps1` **after** QSPI apply, **before** `configure.sh`.

### 2. Build kconfig-tweak (production + test)

In the same `kconfig-tweak` block as QSPI/VelaGuard flags:

- [ ] Always enable UART7 RS485 stack:
  - `--enable CONFIG_STM32H7_UART7`
  - `--enable CONFIG_UART7_RS485`
  - `--set-val CONFIG_UART7_RS485_DIR_POLARITY 1`
  - `--set-val CONFIG_UART7_BAUD 9600`
  - `--set-val CONFIG_UART7_BITS 8`
  - `--set-val CONFIG_UART7_PARITY 0`
  - `--set-val CONFIG_UART7_2STOP 0`
  - `--disable CONFIG_UART7_RXDMA`
  - `--disable CONFIG_UART7_TXDMA`
  - `--enable CONFIG_FS_LINKS`
- [ ] When `productModeFlag=0` (test): enable `CONFIG_EXAMPLES_SERIALRX` / `SERIALBLASTER` and set both `*_DEVPATH` to `/dev/rs485`.
- [ ] When production: ensure both examples remain disabled (explicit `--disable` if base defconfig ever enables them).

### 3. Static / build verification (Gate A)

- [ ] Extend or add harness checks (prefer small addition under `harness/`, e.g. RS485 section in existing flow or dedicated script) to assert:
  - UART7 RS485 patch applies cleanly and is idempotent with QSPI patch still detecting correctly.
  - Production `nuttx.config` (or last build artifact): USART3 console 115200; UART7 on; RS485 on; DMA off; baud 9600; `FS_LINKS=y`; examples off.
  - Test config: examples on; DEVPATH `/dev/rs485`.
  - Guard: with USART3+UART7 only, non-console device is UART7 → documented `/dev/ttyS1` target for `/dev/rs485`.
- [ ] Run production and test builds (or document exact commands) and capture configs under `.debug` as evidence.

### 4. Windows peer script

- [ ] Add `scripts/windows_rs485_peer.ps1`:
  - Params: `-ComPort`, `-Direction` (`PcToBoard`|`BoardToPc`), `-Baud` default 9600, timeout.
  - Payload: exact 26-byte `abcdefghijklmnopqrstuvwxyz`.
  - Fail on wrong length/content/timeout; exit non-zero.
- [ ] Document matching board commands:
  - PC→board: board `serialrx /dev/rs485 26` (or default path); PC script sends.
  - Board→PC: board `serialblaster /dev/rs485 26`; PC script receives/verifies.

### 5. Documentation

- [ ] Write `docs/velaguard-rs485-uart7.md` (pin map, BOM, common ground, SM712, 120 Ω both ends, A/B swap, Gate A/B checklists, device path, commands).
- [ ] README: single link to that doc under docs section.

### 6. Hardware Gate B (blocked on parts; not required to archive foundation if deferred)

- [ ] Wiring checklist execution when hardware arrives.
- [ ] Bidirectional 26-byte pass; DIR returns to receive; COM7 + SWD still work.
- [ ] If parts unavailable at finish: create follow-up task, copy Gate B checklist, archive this task after Gate A only.

## Validation Commands

```bash
# Patch idempotency (from contest repo)
bash scripts/apply-openvela-qspi-patch.sh
bash scripts/apply-openvela-uart7-rs485-patch.sh
bash scripts/apply-openvela-uart7-rs485-patch.sh   # second run: already applied
bash scripts/apply-openvela-qspi-patch.sh          # still already applied

# Build (Windows host typical path; adjust distro/paths as needed)
# production:
#   .\scripts\windows_build_openvela.ps1 -VelaGuardMode production
# test:
#   .\scripts\windows_build_openvela.ps1 -VelaGuardMode test -DebugBuild

# Config greps on artifact
grep -E 'CONFIG_(STM32H7_UART7|UART7_|USART3_|FS_LINKS|EXAMPLES_SERIAL)' .debug/nuttx.config

# Harness (exact entrypoint finalized during implement)
python3 harness/qspi_boot_flow_check.py   # must still pass QSPI checks
# plus any new RS485 static checks
```

On-target (test image, after flash; Gate A software presence only without bus HW):

```text
nsh> ls /dev
# expect ttyS0, ttyS1, rs485 (when FS_LINKS + bringup ran)
nsh> serialrx /dev/rs485 26
nsh> serialblaster /dev/rs485 26
```

## Risky Files / Rollback Points

| Risk | File | Rollback |
|------|------|----------|
| Break QSPI apply detection | Combined patch by mistake | Keep patches separate; never merge |
| Wrong DIR polarity / init level | `board.h` DIR define | `GPIO_OUTPUT_CLEAR` + polarity 1 |
| tty renumber if extra UART enabled | kconfig + symlink target | Guard + docs; update link |
| Softlink fails without `FS_LINKS` | build tweak + bringup | Enable `CONFIG_FS_LINKS` first |
| Production image ships bench tools | test-only kconfig branch | Explicit production disable |
| Build script splat / PowerShell bugs | `windows_build_openvela.ps1` | Minimal edit; no unrelated flash-arg work |

## Review Gates Before `task.py start`

- [x] `prd.md` requirements + two-gate acceptance + doc home decided
- [x] `design.md` boundaries, contracts, patch split, config matrix
- [ ] User reviews artifacts and approves implementation
- [ ] `implement.jsonl` / `check.jsonl` curated (non-seed) for OpenCode sub-agents

## Follow-ups (not this implementation pass)

- Gate B hardware task when BOM arrives
- Issue #02 nanoMODBUS path using `/dev/rs485` only
- Isolated RS485 upgrade (same PB4/PA8/PK1)
