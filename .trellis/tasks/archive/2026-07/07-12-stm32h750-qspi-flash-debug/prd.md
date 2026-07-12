# Stable STM32H750 QSPI flash and debug workflow

## Goal

Provide a reproducible Windows VSCode workflow for building openvela in WSL,
programming an STM32H750B-DK whose firmware exceeds the 128 KiB on-die Flash,
and debugging the resulting QSPI-XIP image without relying on OpenOCD to write
the external NOR.

## Background

- STM32H750XBH6 has 128 KiB of on-die Flash at
  `0x08000000..0x0801ffff`; the board's stock `flash.ld` incorrectly exposes
  2 MiB for the non-M4 build.
- A previous 365 KiB image failed in OpenOCD at `0x08020000`, which is the
  first byte beyond physical internal Flash.
- Historical commit `5eb5070` records hardware validation that xPack OpenOCD
  `stmqspi` could not JEDEC-probe/write the board's dual MT25TL01G, while
  CubeProgrammer's `MT25TL01G_STM32H750B-DISCO.stldr` External Loader worked.
- The current clean-restart branch intentionally has no VelaGuard business
  application yet. The workflow must therefore build the stock
  `stm32h750b-dk:lvgl` target now and remain usable when a team-owned app is
  added later.

## Requirements

- R1: Keep all maintained project changes inside this contest repository;
  represent required upstream NuttX changes as an idempotent patch applied by
  the build script.
- R2: Build a minimal internal-Flash boot stub at `0x08000000` and reject it
  if it exceeds the physical 128 KiB range.
- R3: Link the main NuttX image into the external QSPI XIP window beginning at
  `0x90000000`; reject images outside the configured QSPI range before any
  programming command runs.
- R4: Use Windows STM32CubeProgrammer CLI plus the board-specific External
  Loader to program and verify the QSPI image, then program and verify the
  internal boot stub.
- R5: Use Windows OpenOCD only as the Cortex-Debug GDB server; debugging must
  attach with symbols and must not implicitly rewrite QSPI.
- R6: Provide VSCode tasks for build, release flash, debug flash, flash-only,
  and attach-debug flows when the repository is opened through its Windows
  WSL UNC path.
- R7: Tool paths, WSL distribution, openvela root, output directory, and
  External Loader path must be overridable instead of requiring source edits.
- R8: Preserve the current source tree and existing user changes; do not
  restore the deleted historical VelaGuard application or modify unrelated
  parent repositories.
- R9: Document the tool boundary, artifact addresses, setup, recovery path,
  and diagnostics in Chinese.
- R10: Keep the built-in FT5X06 touchscreen compatible with LVGL 9.2.1 by
  implementing the standard `TSIOC_GETMAXPOINTS` ioctl in the legacy driver;
  return one point for `CONFIG_FT5X06_SINGLEPOINT` and the hardware maximum
  otherwise.
- R11: Replace the stock `lvgldemo` runtime experiment with a team-owned LVGL
  `vscode_lab` application that starts automatically while preserving NSH.
- R12: Expose stable state and a non-inlined checkpoint so a Windows VS Code
  debug session can prove source breakpoints and variable inspection through
  an on-screen touch action.

## Acceptance Criteria

- [x] AC1: The patch applies cleanly and idempotently to the current clean
  openvela/NuttX workspace without leaving untracked changes in unrelated
  repositories.
- [x] AC2: A QSPI build produces `nuttx.elf`, `nuttx.hex`, `nuttx.bin` and
  `qspi_bootstub.{elf,hex,bin}`; static inspection places the main image in
  `0x90000000..0x97ffffff` and the stub in `0x08000000..0x0801ffff`.
- [x] AC3: Flash scripts fail before connecting to hardware when artifacts,
  CubeProgrammer, or the External Loader are missing, or when either HEX file
  has an invalid address range.
- [x] AC4: The CubeProgrammer command programs QSPI with `-el`, programs the
  internal stub without `-el`, verifies both, and resets only after both steps
  succeed.
- [x] AC5: The Cortex-Debug configuration uses the QSPI ELF for symbols,
  OpenOCD's `stm32h750b-disco.cfg`, Windows source mapping, and no GDB load
  operation that rewrites target Flash.
- [x] AC6: Static harness, shell syntax checks, PowerShell parser checks, boot
  stub compilation, contest flow check, and relevant build validation pass.
- [x] AC7: On hardware, cold boot reaches a PC in `0x9000xxxx`, serial output
  reaches NSH or the configured application, and a source breakpoint can be
  hit after attach. Hardware-only checks are explicitly reported if the board
  or Windows tools are unavailable in the execution environment.
- [x] AC8: `lvgldemo widgets` opens `/dev/input0` without the
  `TSIOC_GETMAXPOINTS`/`ENOTTY` error and touch press, release, and drag events
  affect the on-screen widgets with coordinates matching the display.
- [x] AC9: The project-owned VS Code Lab app builds instead of the stock
  `lvgldemo`, starts automatically after reset, updates its heartbeat, button
  count, and slider value, and leaves an interactive NSH console available.
- [x] AC10: The debug ELF exports the documented checkpoint/state symbols and
  Cortex-Debug attach hits the checkpoint when the physical button widget is
  touched without performing any GDB/OpenOCD download.

## Out of Scope

- Making OpenOCD's `stmqspi` implementation program this dual-QSPI device.
- Changing production Option Bytes or relying on direct ROM boot from QSPI.
- Recreating the VelaGuard application or implementing product features.
