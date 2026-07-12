# Validation Record

## Software validation

- Project-owned VS Code Lab release image:
  `0x90000000..0x9004fcec`, 326,892 bytes of QSPI text/data.
- Final VS Code Lab debug HEX:
  `0x90000000..0x900917f0`, 581.98 KiB programmed data.
- Internal boot stub: `0x08000000..0x080002d0`, 720 bytes.
- Debug build contains symbols and uses the same QSPI execution window.
- QSPI harness, Bash syntax, Python compilation, PowerShell parsing, VS Code
  JSON parsing, Cube `-ValidateOnly`, and patch idempotency checks pass.
- Contest flow harness: `119 pass, 6 warn, 0 fail`.
- The final config selects `vscode_lab_main`, disables the stock `lvgldemo`
  application and LVGL example/demo options, and retains `-g3`/no-opt symbols.
- The debug ELF exports `vscode_lab_main`,
  `vscode_lab_debug_checkpoint`, and `g_vscode_lab_debug_state` in the
  expected QSPI/SRAM regions.

## Hardware validation — 2026-07-12

Probe and target:

- Board: STM32H750B-DK.
- ST-LINK serial: `0043001D3137511133333639`.
- ST-LINK firmware: `V3J16M9`.
- Target voltage: approximately 3.27 V.
- CubeProgrammer reports the physical internal Flash as 128 KiB.

Programming through the Windows CubeProgrammer CLI succeeded:

1. `nuttx.hex` was programmed at `0x90000000` with
   `MT25TL01G_STM32H750B-DISCO.stldr` and verified successfully.
2. `qspi_bootstub.hex` was programmed at `0x08000000` without the External
   Loader and verified successfully.
3. The target reset completed successfully.

Runtime and debug evidence:

- After reset and one second of execution, OpenOCD attach halted at
  `PC=0x900015fe`, proving execution reached the external-QSPI XIP image.
- The ST-LINK virtual COM port (`COM7`, 115200 8N1) printed
  `NuttShell (NSH)` and the `nsh>` prompt.
- Windows GDB loaded `.debug/nuttx.elf`, attached through OpenOCD, set a
  hardware source-symbol breakpoint in `nsh_main`, and hit it at
  `PC=0x9000b536`.
- OpenOCD was used only for attach/debug; programming remained owned by
  CubeProgrammer.
- Starting `lvgldemo widgets` opened `/dev/input0` successfully and reported
  `maxpoint 1`; the previous `TSIOC_GETMAXPOINTS` failure (`errno=25`) was no
  longer present.
- Physical interaction confirmed that touch press/release, widget clicks, and
  slider drag events work and that the coordinates match the display.
- The project-owned VS Code Lab starts automatically after reset without an
  NSH command. Its uptime advanced, the custom screen rendered, and the
  project slider followed physical touch input.
- A hardware breakpoint hit `vscode_lab_debug_checkpoint()` at
  `PC=0x9001a0cc` after the physical `Breakpoint +1` button was touched. GDB
  observed `button_count=1`, `slider_value=59`, and `uptime_seconds=97`; the
  stack traced through the LVGL input reader/event dispatcher into the team
  callback.

Runtime defects found and fixed during this validation:

- The boot stub originally required an 8-byte-aligned initial MSP. The new
  image produced the valid word-aligned value `0x24003d94`, so the stub
  rejected it and remained at `0x080002aa`. The guard now requires 4-byte
  alignment and the target reaches QSPI (`PC=0x90002936`).
- The first app version overrode LVGL's framebuffer path with `/dev/lcd0`.
  GDB showed `result.disp=NULL` and a valid input device. Keeping the
  `lv_nuttx_dsc_init()` default selected `/dev/fb0`; the UI then rendered and
  its heartbeat advanced.

## Cold-power validation

The board was manually disconnected from USB power and reconnected. The
internal boot stub initialized memory-mapped QSPI, the external NuttX image
started, and the project-owned VS Code Lab screen appeared automatically.
Together with the verified `PC=0x9000xxxx`, NSH startup, physical touch, and
source-breakpoint results above, this completes AC7 and all hardware acceptance
criteria.
