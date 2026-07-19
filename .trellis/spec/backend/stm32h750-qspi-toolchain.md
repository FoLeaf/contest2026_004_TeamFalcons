# STM32H750 QSPI Build, Flash, and Debug Contract

## Scenario: Windows VS Code workflow for oversized STM32H750 firmware

### 1. Scope / Trigger

This contract applies when building, programming, or debugging openvela on the
STM32H750B-DK from Windows VS Code, especially when the firmware exceeds the
STM32H750XBH6 physical 128 KiB internal Flash.

Source navigation and semantic IntelliSense must run in a VS Code Remote - WSL
window. A Windows-local C/C++ extension opened on a `\\wsl.localhost\...` UNC
folder cannot follow NuttX's Linux absolute symlinks and mixes the Windows
newlib model with NuttX libc types. The supported editor configuration uses the
workspace's Linux `arm-none-eabi-gcc`; do not mask the mismatch with forced
compatibility macros, Windows stub headers, or `-idirafter` ordering tricks.

The supported architecture is always two-stage:

- `0x08000000..0x0801ffff`: internal-Flash QSPI boot stub.
- `0x90000000..0x97ffffff`: external-QSPI NuttX XIP image.

Never enlarge the OpenOCD internal Flash bank to make an oversized image appear
to fit. OpenOCD is the GDB server only; CubeProgrammer owns programming.

### 2. Signatures

Supported entry points:

```powershell
scripts\windows_open_vscode_wsl.ps1
  [-WslDistro <string>]

scripts\windows_build_openvela.ps1
  [-WslDistro <string>]
  [-OpenvelaDir <WSL path>]
  [-BoardConfig <string>]
  [-OutDir <Windows path>]
  [-DebugBuild]
  [-VelaGuardMode test|production]
  [-DeviceIdOverride <1-39 safe characters>]

scripts\windows_flash_cube.ps1
  [-CubeCli <Windows file>]
  [-ExternalLoader <Windows file>]
  [-OutDir <Windows path>]
  [-NoBuild]
  [-DebugBuild]
  [-VelaGuardMode test|production]
  [-DeviceIdOverride <1-39 safe characters>]
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

The VelaGuard application exposes this debug interface:

```c
int velaguard_main(int argc, char *argv[]);
void vg_ui_home_uptime_checkpoint(uint64_t uptime_seconds);
extern volatile uint64_t g_velaguard_uptime_seconds;
```

The older `vscode_lab_main()` interface remains only in `app/hello_app` as a
reference experiment. New product builds must not select it.

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
nuttx.config
build-info.txt
qspi_bootstub.elf
qspi_bootstub.hex
qspi_bootstub.bin
```

PowerShell scripts must forward parameters to another project script with
named hashtable splatting. Do not build a string array containing tokens such
as `"-DebugBuild"` and `"-OutDir"`: for script invocation, those elements can
bind positionally (`WslDistro="-DebugBuild"`) instead of retaining their names.
Switch keys are included with `$true` only when enabled.

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

For the project-owned product, both builds also select
`CONFIG_LVX_USE_VELAGUARD`, set
`CONFIG_INIT_ENTRYPOINT="velaguard_main"`, and disable the old VS Code Lab and
stock `CONFIG_EXAMPLES_LVGLDEMO`, `CONFIG_LV_BUILD_EXAMPLES`, and
`CONFIG_LV_USE_DEMO_WIDGETS` options. `velaguard_main()` must call
`nsh_initialize()` before creating its UI task because that call owns
`BOARDIOC_INIT`; it then enters `nsh_consolemain()` to preserve serial NSH.

Product mode is independent from compiler mode. Test mode sets
`CONFIG_VG_BUILD_MODE=0` and validates a compile-time Device ID containing only
letters, digits, `_`, and `-`, with a maximum of 39 characters. Production
mode sets `CONFIG_VG_BUILD_MODE=1`, ignores the test override, and formats the
STM32H750 96-bit UID as `vg-<24 lowercase hex>`. Neither mode exposes a runtime
Device ID setter.

VelaGuard startup creates `/data/velaguard`, `configs`, and `logs`, then appends
`latest.log` and `events.jsonl`. The boot event contains `device_id`,
`build_mode`, `firmware_version`, `boot_id`, `ts_ms`, `uptime_ms`, and
`time_quality`. A storage failure is fail-visible as `DEGRADED` on serial and
LVGL but must not remove the home screen or NSH rescue console.

The ISSUE1 home screen is a fixed 480x272 dark industrial console. It uses an
asymmetric 304 px local-safety panel and 154 px supporting-state panel, one cyan
product accent, semantic-only red/amber, one 8 px panel radius, and no equal
five-card row, decorative dots, gradients, glows, fake controls, or automatic
animation.

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
| Workspace is opened as a Windows-local UNC folder | Reopen it through Remote - WSL before relying on completion or source navigation. |
| Remote window does not show `WSL: <distro>` or cpptools is only installed locally | Install cpptools in WSL, reset its IntelliSense database, and reload the window. |
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
| A child PowerShell script reports a parameter token as a value, such as WSL distro `-DebugBuild` | Replace string-array forwarding with named hashtable splatting at the caller. |
| Test Device ID is empty, 40+ characters, or contains other punctuation | Reject it before configuring/building and retain an explicit identity error if an invalid value reaches firmware. |
| VelaGuard storage path or either startup append fails | Report the path/error on serial, show `Storage: DEGRADED`, continue LVGL and NSH, and never claim the event was written. |
| Production mode is selected | Derive identity only from the STM32 UID and omit `CONFIG_VG_DEVICE_ID_OVERRIDE` from the generated configuration. |

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
- Good VelaGuard case: test/debug boots from QSPI, uptime advances, storage state
  is honest, NSH remains available, and an attach-only hardware breakpoint hits
  `vg_ui_home_uptime_checkpoint` without downloading.
- Base VelaGuard case: storage is unavailable, the UI and NSH still start, and
  both surfaces report `DEGRADED` rather than silently claiming persistence.
- Bad VelaGuard case: the UI hard-codes `/dev/lcd0`, starts before board
  initialization, exposes a runtime Device ID editor, or hides startup write
  failures.
- Good script-forwarding case: `-DebugBuild` reaches the child as
  `DebugBuild=$true` while `OutDir` retains the real artifact path.
- Bad script-forwarding case: a string array makes `-DebugBuild` the value of
  the child's first positional string parameter.

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
8. For VelaGuard, build `test/debug`, `test/release`, and
   `production/release`. Assert the generated config selects `velaguard_main`,
   keeps product/compiler modes independent, disables the Lab and stock demos,
   and exports the documented identity, startup, and uptime symbols.
9. Run both `harness/velaguard_issue1_check.py` and
   `harness/qspi_boot_flow_check.py` against every retained variant. Confirm the
   QSPI/internal HEX ranges and run Cube `-NoBuild -ValidateOnly` on Windows.
10. After Cube programming, assert the VelaGuard screen appears automatically,
   its uptime advances, startup logs are visible on COM7/NSH, and touch
   initialization does not remove the non-interactive ISSUE1 home. Attach
   without loading and hit `vg_ui_home_uptime_checkpoint`.
11. Flash `production/release`, perform two fully unpowered cold boots, and
   require the same `vg-<24 lowercase hex>` Device ID on both boots.
12. For every project-script forwarding boundary, assert named hashtable
    splatting and reject string-array construction of named arguments. Run the
    Windows debug-flash task and confirm it reports the configured WSL distro
    and a debug build independently.

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

For PowerShell script forwarding:

```powershell
# Wrong: the child can bind these tokens as positional values.
$arguments = @("-DebugBuild", "-OutDir", $OutDir)
& $buildScript @arguments

# Correct: the child binds by declared parameter name.
$parameters = @{ OutDir = $OutDir }
if ($DebugBuild) { $parameters["DebugBuild"] = $true }
& $buildScript @parameters
```

For display initialization:

```c
/* Wrong: assumes the current LVGL backend is LCD. */
lv_nuttx_dsc_init(&info);
info.fb_path = "/dev/lcd0";

/* Correct: let the selected backend choose /dev/fb0 or /dev/lcd0. */
lv_nuttx_dsc_init(&info);
info.input_path = CONFIG_LVX_VELAGUARD_INPUT_PATH;
```

For VelaGuard identity selection:

```text
# Wrong: production image accepts a UI/NSH command that replaces Device ID.
set_device_id site-override

# Correct: product mode is compile-time and production reads UID only.
CONFIG_VG_BUILD_MODE=1
device_id = "vg-" + 24 lowercase UID hex digits
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

## Scenario: MB1381 H750XB-B01 MII Ethernet with QSPI-XIP

### 1. Scope / Trigger

Apply this contract whenever enabling or modifying onboard Ethernet for the
MB1381 H750XB-B01 while the firmware executes from dual-QSPI XIP. The B01
schematic is authoritative: U9 is a LAN8740A on full MII at PHY address 1.
Generic STM32H750B-DK documentation that describes LAN8742A/RMII does not
describe this hardware revision.

### 2. Signatures

The Windows build must invoke this idempotent helper before NuttX configure:

```bash
scripts/apply-openvela-eth-mii-patch.sh <openvela-root>
```

`<openvela-root>` must contain both the `nuttx` and `apps` checkouts. Omitting
it defaults to the contest repository's parent directory. The helper applies
the NuttX MAC/PHY patch and apps netinit patch independently and idempotently.
Physical link edges are published through:

```c
netdev_carrier_on(dev);
netdev_carrier_off(dev);
```

### 3. Contracts

- Select `CONFIG_STM32H7_MII=y`, `CONFIG_STM32H7_MII_EXTCLK=y`,
  `CONFIG_ETH0_PHY_LAN8740A=y`, and `CONFIG_STM32H7_PHYADDR=1`.
- Explicitly disable `CONFIG_ETH0_PHY_LAN8742A`; do not select RMII.
- The LAN8740A supplies the external MII clocks.
- PH2/MII_CRS and PH3/MII_COL share QSPI bank 2 IO0/IO1. When
  `CONFIG_STM32H750B_DK_QSPI_BOOT=y`, define
  `BOARD_ETH_MII_NO_CRS_COL` and leave both pins configured for QSPI.
- Full-duplex Ethernet does not use CRS/COL. The driver must still configure
  the remaining MII pins.
- `IFF_UP` is administrative state and must remain set while the cable is
  absent. `IFF_RUNNING` follows the debounced LAN8740A carrier only.
- For QSPI builds, define `BOARD_ETH_PHY_POLL`. Poll BMSR from LPWORK every
  500 ms, read the latch-low register twice per sample, and require two
  matching samples before publishing an edge. Do not use the incomplete
  STM32H7 PHY-interrupt path or assume a GPIO for `MII_TX_ER_nINT`.
- On a rising edge, rerun PHY negotiation, synchronize the MAC speed/duplex
  bits, then call `netdev_carrier_on()`. On a falling edge, cancel the TX
  watchdog and call `netdev_carrier_off()` without clearing `IFF_UP`.
- Enable `CONFIG_NETINIT_CARRIER_POLL=y`. The netinit thread observes
  `IFF_RUNNING`; carrier loss clears the active IPv4 address, netmask, and
  default router, while carrier recovery starts/retries DHCP in the
  background. Missing cable or DHCP service must not block VelaGuard UI or
  serial NSH.
- Clearing active state does not send DHCP RELEASE after physical loss. The
  stable MAC remains the client identity, but a reconnect may receive either
  the previous address or a different address.

### 4. Validation & Error Matrix

| Condition | Required result |
|---|---|
| Hardware is MB1381 H750XB-B01 | Configure LAN8740A, full MII, external clocks, PHY address 1. |
| README says LAN8742A/RMII | Reject that generic mapping and verify against the B01 schematic. |
| QSPI-XIP is enabled | Preserve PH2/PH3 as QSPI IO; skip only MII CRS/COL GPIO setup. |
| Cable is absent after `ifup` | Keep `UP`, clear `RUNNING`, and continue LPWORK polling; do not fail boot. |
| BMSR read fails | Retain the last confirmed carrier state, log the error, and reschedule polling. |
| Initial LPWORK scheduling fails | Log the error, roll the interface down cleanly, and return the failure. |
| DHCP `sendto()` returns `EHOSTUNREACH` and capture sees no packet | Inspect `IFF_RUNNING`/carrier before PHY traffic or DHCP payload debugging. |
| MII patch is missing, conflicts, or only partially applied | Stop before configure/build and report the exact NuttX checkout. |
| Confirmed carrier loss | Preserve `IFF_UP`; clear IPv4, netmask, and the old default route. |
| Carrier returns but DHCP fails | Keep `UP + RUNNING + 0.0.0.0` and retry at the configured interval. |
| Cable and DHCP service are present | `eth0` obtains a non-`0.0.0.0` IPv4 address and supports bidirectional ping. |

### 5. Good / Base / Bad Cases

- Good: B01 boots from QSPI with LAN8740A/MII, `ifconfig` reports `eth0` as
  `RUNNING` with a DHCP address, PC-to-board and board-to-PC ping both pass,
  and LVGL/touch/NSH remain operational.
- Base: the cable is unplugged; QSPI-XIP, VelaGuard UI, and NSH still run,
  while `eth0` is `UP` without `RUNNING` and reports address/router/netmask as
  `0.0.0.0`.
- Bad: configure RMII/LAN8742A from the generic README, switch PH2/PH3 away
  from QSPI, preserve stale `RUNNING`/IPv4 after unplug, clear `IFF_UP` on a
  physical edge, or require manual `renew` after reconnect.

### 6. Tests Required

13. For every B01 Ethernet change:
    - reverse-check the maintained patch against the target NuttX checkout;
    - assert the generated config selects MII, external clocks, LAN8740A, and
      PHY address 1 while disabling LAN8742A;
    - reverse-check both maintained NuttX and apps patches;
    - run `harness/qspi_boot_flow_check.py --artifacts .debug` and assert it
      checks QSPI pin preservation, double-read BMSR, debounce, both carrier
      calls, IPv4/route cleanup, DHCP reconnect, and preserved `IFF_UP`;
    - flash and cold boot with cable connected, then require a DHCP address
      plus bidirectional ping;
    - cold boot once with the cable unplugged and require UI plus NSH startup;
    - perform at least three unplug/replug cycles without manual `renew`.
      Within about two seconds after unplug require `UP`, no `RUNNING`, and
      zero IPv4/router/netmask; after replug require automatic DHCP and
      bidirectional ping. Do not require the DHCP address to stay unchanged.

### 7. Wrong vs Correct

#### Wrong

```text
CONFIG_STM32H7_RMII=y
CONFIG_ETH0_PHY_LAN8742A=y
configure PH2=MII_CRS and PH3=MII_COL during QSPI-XIP
physical unplug leaves stale RUNNING, IPv4, and default route
netinit clears IFF_UP or reconnect requires manual renew
```

#### Correct

```text
CONFIG_STM32H7_MII=y
CONFIG_STM32H7_MII_EXTCLK=y
CONFIG_ETH0_PHY_LAN8740A=y
CONFIG_STM32H7_PHYADDR=1
QSPI-XIP preserves PH2/PH3; full-duplex MII skips CRS/COL
IFF_UP remains administrative; debounced BMSR edges own IFF_RUNNING
carrier-off clears active IPv4/route; carrier-on automatically retries DHCP
```
