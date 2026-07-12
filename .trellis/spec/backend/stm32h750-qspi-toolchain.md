# STM32H750 QSPI Build, Flash, and Debug Contract

## Scenario: Windows VS Code workflow for oversized STM32H750 firmware

### 1. Scope / Trigger

This contract applies when building, programming, or debugging openvela on the
STM32H750B-DK from Windows VS Code, especially when the firmware exceeds the
STM32H750XBH6 physical 128 KiB internal Flash.

The supported architecture is always two-stage:

- `0x08000000..0x0801ffff`: internal-Flash QSPI boot stub.
- `0x90000000..0x97ffffff`: external-QSPI NuttX XIP image.

Never enlarge the OpenOCD internal Flash bank to make an oversized image appear
to fit. OpenOCD is the GDB server only; CubeProgrammer owns programming.

### 2. Signatures

Supported entry points:

```powershell
scripts\windows_build_openvela.ps1
  [-WslDistro <string>]
  [-OpenvelaDir <WSL path>]
  [-BoardConfig <string>]
  [-OutDir <Windows path>]
  [-DebugBuild]

scripts\windows_flash_cube.ps1
  [-CubeCli <Windows file>]
  [-ExternalLoader <Windows file>]
  [-OutDir <Windows path>]
  [-NoBuild]
  [-DebugBuild]
  [-ValidateOnly]

scripts\windows_flash_openocd.ps1
  # Compatibility entry point with the same flash arguments. It must delegate
  # to windows_flash_cube.ps1 and warn that OpenOCD is debug-only.
```

The build must invoke the idempotent patch interface before configuring NuttX:

```bash
scripts/apply-openvela-qspi-patch.sh <openvela-root>
```

VS Code debug configurations must use Cortex-Debug with `request: "attach"`,
`.debug/nuttx.elf` as the executable/symbol file, and `loadFiles: []`.

The team application exposes this debug interface:

```c
int vscode_lab_main(int argc, char *argv[]);
void vscode_lab_debug_checkpoint(uint32_t button_count,
                                 int32_t slider_value);
extern struct vscode_lab_debug_state g_vscode_lab_debug_state;
```

### 3. Contracts

Inputs and environment overrides:

| Name | Type | Required | Contract |
|---|---|---:|---|
| `OPENVELA_WSL_DISTRO` | string | no | Defaults to `Debian`. |
| `OPENVELA_ROOT_WSL` | WSL path | no | Defaults to the parent of the contest repository. |
| `OPENVELA_OUT_DIR` | Windows path | no | Defaults to `<contest-repo>\.debug`. |
| `STM32_PROGRAMMER_CLI` | Windows file | for flash | CubeProgrammer CLI path; explicit parameter wins. |
| `STM32_EXTERNAL_LOADER` | Windows file | for flash | Must resolve to `MT25TL01G_STM32H750B-DISCO.stldr` or a deliberately validated replacement. |

The build output directory is the single hand-off boundary and must contain:

```text
nuttx.elf
nuttx.hex
nuttx.bin
qspi_bootstub.elf
qspi_bootstub.hex
qspi_bootstub.bin
```

`nuttx.hex` must contain data only within the external-QSPI window.
`qspi_bootstub.hex` must contain data only within the physical internal-Flash
window. Intel HEX upper bounds are exclusive.

The flash sequence is ordered and fail-fast:

1. Validate tools, files, and both HEX ranges without accessing hardware.
2. Program and verify `nuttx.hex` using the External Loader.
3. Program and verify `qspi_bootstub.hex` without the External Loader.
4. Reset the target.

Release builds enable `CONFIG_DEBUG_FULLOPT` and disable debug symbols/no-opt.
Debug builds enable `CONFIG_DEBUG_SYMBOLS`, `-g3`, and `CONFIG_DEBUG_NOOPT`.
Both builds enable `CONFIG_STM32H750B_DK_QSPI_BOOT`.

For the project-owned experiment, both builds also select
`CONFIG_LVX_USE_DEMO_CONTEST2026_004_VSCODE_LAB`, set
`CONFIG_INIT_ENTRYPOINT="vscode_lab_main"`, and disable the stock
`CONFIG_EXAMPLES_LVGLDEMO`, `CONFIG_LV_BUILD_EXAMPLES`, and
`CONFIG_LV_USE_DEMO_WIDGETS` options. `vscode_lab_main()` must call
`nsh_initialize()` before creating its UI task because that call owns
`BOARDIOC_INIT`; it then enters `nsh_consolemain()` to preserve serial NSH.

The boot stub validates that the QSPI vector MSP is inside SRAM and 4-byte
aligned. Do not require 8-byte alignment: NuttX can emit a valid early stack
value such as `0x24003d94` before it establishes ABI-aligned task stacks.

Applications must call `lv_nuttx_dsc_init()` and keep its `fb_path` unless a
board-specific backend is intentionally selected. On this configuration it
chooses `/dev/fb0`; forcing `/dev/lcd0` produces a valid input device but a
null LVGL display.

### 4. Validation & Error Matrix

| Condition | Required result |
|---|---|
| `wsl.exe` is missing | Stop before build with an actionable error. |
| Windows/UNC path cannot convert to WSL | Stop and name the failing path. |
| QSPI patch is neither cleanly applicable nor already applied | Stop before configure/build. |
| `nuttx.hex` has data below `0x90000000` or at/above `0x98000000` | Reject before CubeProgrammer starts. |
| Boot-stub HEX has data below `0x08000000` or at/above `0x08020000` | Reject before CubeProgrammer starts. |
| Required HEX file, Cube CLI, or External Loader is missing | Stop and identify the missing input. |
| `-ValidateOnly` is set | Validate all software inputs and exit without opening ST-LINK. |
| QSPI program/verify fails | Stop; do not program the stub or reset. |
| Internal-stub program/verify fails | Stop; do not report success or continue to reset. |
| No ST-LINK is connected | Report hardware validation as pending, never as passed. |
| Debug is launched | Attach with symbols; never download from GDB/OpenOCD. |
| GDB sets a QSPI source breakpoint while reset-halted in the internal stub | The read of `0x9000...` may fail because QSPI is not memory-mapped yet; first attach after QSPI startup, set a hardware breakpoint, then reset/run if startup coverage is required. |
| LVGL reports `get touch maxpoints failed (errno=25)` | The touchscreen driver is missing `TSIOC_GETMAXPOINTS`; fix the driver contract rather than masking `ENOTTY` in LVGL. |
| `TSIOC_GETMAXPOINTS` receives a null output pointer | Return `-EINVAL`; do not dereference it. |
| Boot stub remains at `0x080002aa` with a word-aligned QSPI vector MSP | Do not reject `(msp & 4) != 0`; require only `(msp & 3) == 0` plus the SRAM range. |
| `lv_nuttx_init()` returns `disp=NULL` but `indev!=NULL` | Check for an incorrect display-path override; retain the `/dev/fb0` default on the current framebuffer config. |
| Custom app starts before `BOARDIOC_INIT` | Run `nsh_initialize()` before creating the UI task; otherwise display/input registration races the app. |

Normal scripts must not alter Option Bytes, mass erase the device, or claim an
OpenOCD flash bank larger than the chip's physical internal Flash.

### 5. Good / Base / Bad Cases

- Good: a debug build emits a symbolized `nuttx.elf` at `0x90000000`, Cube
  programs both images, cold boot reaches `0x9000xxxx`, and OpenOCD attaches to
  a working source breakpoint without downloading.
- Base: with no board attached, build plus `-NoBuild -ValidateOnly` proves the
  tool paths, artifact presence, and address layout; hardware acceptance stays
  pending.
- Bad: an oversized `0x08000000` image is sent to OpenOCD, an arbitrary QSPI
  driver is expected to probe the board's dual MT25TL01G arrangement, or an
  External Loader is used without checking the HEX address window.
- Good custom-app case: uptime advances, slider touch changes the state, and a
  hardware breakpoint hit reports the same counter/value arguments shown on
  screen while NSH remains available.
- Bad custom-app case: the UI hard-codes `/dev/lcd0`, starts before board
  initialization, or a boot guard rejects a word-aligned NuttX initial MSP.

### 6. Tests Required

For any change to this workflow, assert all applicable points:

1. Parse `.vscode/*.json` and all three PowerShell scripts successfully.
2. Run Bash syntax checks and Python compilation for changed helpers.
3. Build release and debug variants; inspect ELF/HEX ranges and confirm debug
   sections are present only where intended.
4. Run `python3 harness/qspi_boot_flow_check.py --artifacts .debug`; every
   assertion must pass.
5. Run `windows_flash_cube.ps1 -NoBuild -ValidateOnly` on Windows; confirm it
   does not access ST-LINK.
6. With hardware connected, program both images, power-cycle the board, verify
   serial/NSH startup and a PC in `0x9000xxxx`, then attach OpenOCD and hit a
   source breakpoint.
7. Start `lvgldemo widgets`; assert that `/dev/input0` opens without `ENOTTY`,
   then physically verify press/release, a button click, a slider drag, and
   coordinate alignment near the display edges.
8. For the project app, assert the generated config selects `vscode_lab_main`
   and disables stock demo/example options. Verify the ELF exports the three
   documented debug symbols.
9. After Cube programming, assert the custom screen appears automatically and
   its uptime advances. Attach without loading, set a hardware breakpoint on
   `vscode_lab_debug_checkpoint`, touch the button, and compare the arguments
   with `g_vscode_lab_debug_state`.

For a breakpoint in early QSPI-resident startup code, let the target reach the
QSPI image once before asking GDB to resolve/read the source location. Set a
hardware breakpoint while QSPI is mapped, then reset and continue. A breakpoint
attempt made while halted in the internal boot stub is not a valid attach-flow
failure because the external address window does not exist yet.

Software-only checks do not substitute for step 6. Record the exact hardware
blocker when it cannot run.

### 7. Wrong vs Correct

#### Wrong

```text
# Pretend the H750 has 2 MiB internal Flash, then ask OpenOCD to download an
# oversized image beginning at 0x08000000.
flash bank stm32h7x 0x08000000 0x200000 ...
```

This violates the silicon capacity and does not solve external-QSPI loading.

#### Correct

```text
CubeProgrammer + MT25TL01G_STM32H750B-DISCO.stldr
  -> verify NuttX XIP HEX at 0x90000000
CubeProgrammer
  -> verify boot stub at 0x08000000
OpenOCD + Cortex-Debug request=attach + loadFiles=[]
  -> debug using .debug/nuttx.elf symbols only
```

The External Loader contains the board-specific QSPI programming algorithm;
the boot stub initializes memory-mapped QSPI for execution after reset.

For display initialization:

```c
/* Wrong: assumes the current LVGL backend is LCD. */
lv_nuttx_dsc_init(&info);
info.fb_path = "/dev/lcd0";

/* Correct: let the selected backend choose /dev/fb0 or /dev/lcd0. */
lv_nuttx_dsc_init(&info);
info.input_path = CONFIG_LVX_USE_DEMO_CONTEST2026_004_VSCODE_LAB_INPUT_PATH;
```

## FT5X06 contract required by LVGL 9.2.1

The legacy `drivers/input/ft5x06.c` character driver must implement:

```c
case TSIOC_GETMAXPOINTS:
  /* arg is a non-null uint8_t output pointer */
  /* return 1 for CONFIG_FT5X06_SINGLEPOINT, else FT5X06_MAX_TOUCHES */
```

LVGL queries this ioctl before registering its input device. A successful
`open()` or valid I2C traffic is not sufficient: returning `-ENOTTY` here
causes LVGL input creation to stop before normal reads begin. Keep this fix at
the driver boundary so every standards-compliant touchscreen consumer sees
the same capability.

Wrong: let LVGL ignore an arbitrary ioctl failure and guess one touch point.

Correct: validate `(uint8_t *)(uintptr_t)arg`, return `-EINVAL` for null, and
write the configured FT5X06 point count before returning success. The static
QSPI harness must assert that the maintained upstream patch contains this
case, and the final acceptance test must include real press/release and drag
interaction rather than relying only on the successful-open log.
