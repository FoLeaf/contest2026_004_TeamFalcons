# VS Code Lab Design

## Goal

Replace the stock `lvgldemo` runtime experiment with a team-owned LVGL
application that starts automatically on STM32H750B-DK and makes the complete
Windows VS Code build, CubeProgrammer flash, and OpenOCD attach workflow easy
to verify.

## Chosen approach

The application remains in the existing team-owned `app/hello_app` manifest
slot to avoid changing workspace manifests, but its program and user-facing
identity become `vscode_lab`. The Windows build wrapper enables this app,
disables the stock LVGL demo application, and changes `CONFIG_INIT_ENTRYPOINT`
to `vscode_lab_main`.

`vscode_lab_main` runs `nsh_initialize()` first so `BOARDIOC_INIT` registers
the LCD and touchscreen, creates a dedicated LVGL UI task, and then enters
`nsh_consolemain()`. The display therefore starts without a serial command
while NSH remains available on the ST-LINK virtual COM port.

Alternatives rejected:

- An NSH `rcS` script adds ROMFS/startup-script dependencies to a board config
  that does not currently use them.
- Starting the app from `board_late_initialize()` couples a team experiment to
  upstream STM32 board code and makes rollback harder.

## Application components

`app/hello_app` will contain:

- `Kconfig`: `LVX_USE_DEMO_CONTEST2026_004_VSCODE_LAB`, stack size, priority,
  and touchscreen path. The display path comes from `lv_nuttx_dsc_init()` so
  it follows the selected framebuffer/LCD backend.
- `CMakeLists.txt`, `Makefile`, and `Make.defs`: register the built-in program
  as `vscode_lab`, with LVGL as a dependency.
- `vscode_lab_main.c`: boot hand-off, LVGL initialization/event loop, UI, touch
  callbacks, and serial diagnostics.

The initial UI contains:

- title and fixed firmware label;
- heartbeat/uptime label proving the event loop is alive;
- touch counter;
- `Breakpoint +1` button;
- slider and numeric slider-value label;
- short on-screen instructions for the VS Code experiment.

## Debug contract

The source exports these stable debug points:

```c
struct vscode_lab_debug_state
{
  volatile uint32_t button_count;
  volatile int32_t slider_value;
  volatile uint32_t uptime_seconds;
};

extern struct vscode_lab_debug_state g_vscode_lab_debug_state;

void vscode_lab_debug_checkpoint(uint32_t button_count,
                                 int32_t slider_value);
```

`vscode_lab_debug_checkpoint()` is non-static and `noinline`. Every button
click updates `g_vscode_lab_debug_state`, logs the values, and calls the
checkpoint. A debug ELF must retain both symbols so Cortex-Debug can set a
source or function breakpoint before the user touches the button.

The slider updates its label and debug state. It logs only on release to avoid
flooding the serial port while dragging.

## Startup and data flow

```text
NuttX init
  -> vscode_lab_main
       -> nsh_initialize (board drivers)
       -> task_create(vscode_lab_ui)
       -> nsh_consolemain (serial shell remains available)

vscode_lab_ui
  -> board/LVGL initialization
  -> /dev/lcd0 + /dev/input0
  -> create widgets
  -> lv_timer_handler loop

touch button
  -> increment state
  -> update label
  -> vscode_lab_debug_checkpoint (VS Code breakpoint)
```

Only the UI task calls LVGL. NSH and debug reads may observe the volatile
state but do not mutate LVGL objects.

## Failure handling

- Failure to create the UI task is printed to the serial console; NSH still
  starts so the target remains diagnosable.
- Failure to initialize a display is logged and the UI task exits without
  taking down NSH.
- A second manual `vscode_lab` invocation is rejected using a guarded running
  flag, preventing two LVGL owners from opening the same devices.
- The existing QSPI address validation and Cube/OpenOCD tool boundaries remain
  unchanged.
- The boot stub accepts a word-aligned NuttX initial MSP. It must not impose
  an 8-byte vector-value check because valid NuttX image layout can produce an
  early stack such as `0x24003d94` before normal ABI-aligned task stacks are
  established.

## Build integration

The Windows build wrapper will configure `stm32h750b-dk:lvgl`, then:

1. enable `CONFIG_LVX_USE_DEMO_CONTEST2026_004_VSCODE_LAB`;
2. disable `CONFIG_EXAMPLES_LVGLDEMO`;
3. disable `CONFIG_LV_BUILD_EXAMPLES` and `CONFIG_LV_USE_DEMO_WIDGETS` so
   stock LVGL example/demo sources are not linked into the experiment;
4. set `CONFIG_INIT_ENTRYPOINT="vscode_lab_main"`;
5. retain the QSPI, touchscreen, NSH, release, and debug settings already
   validated by the project.

The team app is already exposed to openvela through the contest workspace's
`packages/demos/contest2026_004_hello_app` link. The maintained directory name
is legacy scaffold metadata; no openvela demo source is reused.

## Validation

Software checks:

1. C/CMake/Kconfig static checks and project harness pass.
2. Release and debug QSPI builds succeed.
3. `nuttx.hex` remains entirely inside `0x90000000..0x97ffffff` and the boot
   stub remains inside the physical 128 KiB internal Flash.
4. `arm-none-eabi-nm .debug/nuttx.elf` contains `vscode_lab_main`,
   `vscode_lab_debug_checkpoint`, and `g_vscode_lab_debug_state`.
5. The generated config selects the team app, uses its init entrypoint, and
   does not select `CONFIG_EXAMPLES_LVGLDEMO`.

Hardware checks:

1. CubeProgrammer programs and verifies both images.
2. The VS Code Lab screen appears automatically after reset without entering
   `lvgldemo` or another NSH command.
3. Uptime changes, the button increments, and the slider follows touch input.
4. COM7 still reaches an `nsh>` prompt and prints application events.
5. Cortex-Debug attach hits `vscode_lab_debug_checkpoint()` when the button is
   pressed, with state values visible in the debugger.

## Out of scope

- Product-specific VelaGuard behavior.
- Replacing the existing QSPI boot architecture.
- Teaching OpenOCD to program the dual external QSPI device.
- Additional screens, persistence, networking, or production UI styling.
