# Design: STM32H750 QSPI flash and debug workflow

## Architecture

The build produces two independently validated images. A freestanding boot
stub occupies the real 128 KiB internal Flash window and initializes the board
QSPI controller in the hardware-validated SPI memory-mapped mode before
jumping through the vector table at `0x90000000`. NuttX is linked as an XIP
image at `0x90000000`; its writable data retains SRAM VMAs with QSPI load
addresses.

The team repository owns a patch that adds a QSPI Kconfig switch, linker
script selection for Make/CMake, and an executable MPU mapping to upstream
NuttX. A guarded patch helper applies it to the exact clean baseline. This
keeps the public NuttX checkout free of hand-maintained edits while allowing
the complete workspace build to consume the support.

## Tool Boundaries and Data Flow

1. Windows VSCode invokes a PowerShell build wrapper.
2. The wrapper invokes WSL, applies the NuttX patch, configures
   `stm32h750b-dk:lvgl`, enables QSPI boot and optional debug symbols, builds
   NuttX, builds the boot stub, and copies artifacts to a Windows directory.
3. The flash wrapper parses both Intel HEX files before accessing hardware.
4. CubeProgrammer loads `MT25TL01G_STM32H750B-DISCO.stldr`, writes/verifies the
   main HEX in external QSPI, disconnects, writes/verifies the boot stub in
   internal Flash, and resets.
5. Cortex-Debug starts OpenOCD as a debug server and attaches GDB using the
   QSPI ELF solely as a symbol file. The debug configuration suppresses target
   download so CubeProgrammer remains the only programming authority.

## Interfaces

- `scripts/windows_build_openvela.ps1`: parameters for WSL distro, openvela
  root, board config, output directory, and debug build; QSPI is the safe
  default for this board.
- `scripts/windows_flash_cube.ps1`: parameters for Cube CLI, External Loader,
  artifact directory, optional build, and debug build.
- `scripts/apply-openvela-qspi-patch.sh`: idempotent patch application with a
  clean/already-applied/error result.
- `.vscode/tasks.json` and `.vscode/launch.json`: user-facing build/flash and
  attach-debug entries; machine-specific paths are supplied through
  environment variables with historical paths as backward-compatible
  defaults.

## Failure Handling and Safety

- Address-range validation happens before CubeProgrammer is launched.
- The main image may not overlap internal Flash; the stub may not extend past
  `0x0801ffff`.
- Each CubeProgrammer step checks its exit status; the stub and reset are not
  attempted after a failed QSPI write.
- No script changes Option Bytes, performs a mass erase by default, or lies to
  OpenOCD about internal Flash size.
- The old OpenOCD flash entry becomes an explicit diagnostic/compatibility
  wrapper that redirects to CubeProgrammer or clearly refuses QSPI writes.

## Compatibility and Rollback

The implementation targets the current `dev-ai-contest-2026` NuttX baseline
and Windows VSCode opened through `\\wsl.localhost`. The stock board build
remains available when the QSPI Kconfig switch is disabled. Rollback consists
of removing the team-owned scripts/configuration and reversing the patch in
the NuttX checkout; no Option Byte migration is required.

## FT5X06 / LVGL Input Compatibility

LVGL 9.2.1 requires touchscreen devices to answer
`TSIOC_GETMAXPOINTS` before it creates an input device. The board's existing
legacy FT5X06 character driver only handles frequency ioctls and returns
`-ENOTTY`, despite successfully opening and reading the hardware.

The compatibility fix stays at the driver contract boundary: add
`TSIOC_GETMAXPOINTS` to `ft5x06_ioctl()`, validate the output pointer, return
one point under `CONFIG_FT5X06_SINGLEPOINT`, and return
`FT5X06_MAX_TOUCHES` otherwise. This is intentionally smaller than migrating
the established driver to the newer touchscreen upper-half, and safer than
teaching LVGL to assume a point count after arbitrary ioctl failures. The
change is carried by the same team-owned, idempotent NuttX patch.

## Team-owned VS Code Lab

The final experiment no longer uses the stock `lvgldemo` UI. The existing
team application manifest slot hosts a built-in program named `vscode_lab`.
Its init entrypoint first runs `nsh_initialize()` to register board devices,
creates one LVGL UI task, and then enters `nsh_consolemain()`, so the screen
appears automatically without racing LCD/touch initialization or sacrificing
serial recovery.

The UI provides uptime, a button counter, and a slider. The button callback
updates a global volatile debug-state structure and calls the stable,
non-inlined `vscode_lab_debug_checkpoint()` function. These symbols form the
explicit Cortex-Debug experiment contract. Only the UI task owns LVGL; NSH
and GDB only observe state.

The Windows build wrapper enables the team app, disables
`CONFIG_EXAMPLES_LVGLDEMO`, and selects `vscode_lab_main` as the init
entrypoint. QSPI placement, CubeProgrammer ownership, and attach-only OpenOCD
behavior remain unchanged. The detailed reviewed design is
`docs/superpowers/specs/2026-07-12-vscode-lab-design.md`.
