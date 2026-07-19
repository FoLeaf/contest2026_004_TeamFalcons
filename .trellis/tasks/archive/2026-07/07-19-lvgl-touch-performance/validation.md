# Validation

## Software checks

- PASS: Bash syntax, Python compilation, and UI-performance static harness.
- PASS: ISSUE1, settings/network, and QSPI artifact harnesses.
- PASS: maintained UI-performance patch reverse check and idempotent helper.
- PASS: QSPI helper recognizes a fully applied patch when the later FT5X06
  performance patch overlaps its driver hunk.
- PASS: final STM32H750B-DK ARM build with diagnostics disabled and interrupt
  touch mode; `CONFIG_LV_DEF_REFR_PERIOD=20`,
  `CONFIG_LV_NUTTX_VSYNC_TIMER_PERIOD=20`, priority 120, `-Og`, and `-g3`.
- PASS: prior diagnostic-enabled and fast-poll fallback ARM builds linked.
- PASS: generated artifact checks for QSPI HEX ranges and VelaGuard symbols.
- PASS: no generated `.built`, `.depend`, or `Make.dep` markers remain in the
  application source directory.
- PASS: physical `tc` sampling at 100 kHz + fast-poll produced a continuous
  in-range MOVE/UP sequence across the 480x272 panel.
- FAIL/ROLLED BACK: 400 kHz sampling produced sparse, out-of-range data such
  as `x=3630`; the maintained patch now retains the board-validated 100 kHz.

## Hardware checks pending

The board and Windows CubeProgrammer/PowerShell environment were unavailable
in this session. These acceptance items remain pending and must not be inferred
from software checks:

- Release 60-second FPS and frame-time P95 after warm-up.
- 100 press feedback measurements in Release and Debug, including missed
  down/up events.
- 20 drag gestures, 50 navigation cycles, and network/service-load run.
- Interrupt-mode reliability, cold boots, NSH, and QSPI display integrity.
- CubeProgrammer `-NoBuild -ValidateOnly` on Windows followed by physical flash.

If interrupt validation finds stuck or missing events, rebuild with
`-FastTouchPoll` and repeat the complete touch matrix.

## Close-out result

- PASS: final project harnesses, Bash/Python syntax, patch reverse-check, and
  idempotent patch application were rerun before close-out.
- PASS: the new diagnostics sources follow the project NuttX-style section
  layout. The NuttX repository `checkpatch.sh` cannot validate files outside
  the NuttX Git root and reports only that repository-boundary error for these
  application paths.
- FAIL: physical user feedback after both interrupt and fast-poll flashes says
  scrolling remains poor and the LVGL frame rate remains low. The original
  product-performance goal and physical FPS/latency acceptance criteria were
  not achieved by this first-stage scheduling/touch optimization.
- HANDOFF: continue in a new Trellis task covering always-visible native LVGL
  FPS/CPU monitoring, asynchronous VSYNC page flips, SDRAM framebuffer
  reservation, STM32H7 DMA2D support, an LVGL DMA2D draw unit, and a sustained
  50 FPS scrolling target. Do not represent this task as hardware-accepted.
