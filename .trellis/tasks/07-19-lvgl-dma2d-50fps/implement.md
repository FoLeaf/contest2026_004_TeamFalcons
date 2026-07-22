# Implementation Plan

1. Add a regression harness for monitor settings, 16 ms scheduling, SDRAM
   reservation, asynchronous pan-display ownership, DMA2D build integration,
   draw-unit fallback rules, cache operations, and rollback metadata.
2. Update the build/flash parameter chain: fast-poll default, interrupt touch,
   DMA2D disable, monitor hide, and complete `build-info.txt` fields.
3. Enable native LVGL sysmon/performance monitoring and NuttX CPU-load support
   in every normal configuration; retain the synthetic diagnostic separately.
4. Reserve the SDRAM prefix from the H7 heap and make LTDC VBR reload
   asynchronous with ISR-owned pan-buffer release and an explicit pending gate.
5. Reserve H7 framebuffer memory and add an application-owned DMA2D unit with
   RCC/reset/IRQ control, bounded error handling, and no dependency on an
   incomplete shared lower-half port.
6. Add the LVGL STM32 DMA2D draw unit with a 256-pixel dispatch threshold for
   opaque RGB565 fills, cache maintenance, GPU timing, serial counters, and
   software fallback on unsupported or failed tasks.
7. Flatten repeated settings/network scrolling rows and preserve static
   control/dialog radii; keep change-detection on periodic UI values.
8. Split and harden project-owned NuttX/apps patch helpers, then verify forward,
   reverse, repeated, partial, and overlapping application behavior.
9. Build `test/debug`, run all static/artifact harnesses, inspect `.config`,
   ELF/map, memory ranges, and DMA2D symbols; repeat for both rollback backends.
10. Build `test/release` and `production/release`, flash hardware, and execute
    the FPS/CPU, reference-render, touch, tearing, heap, and network-load
    acceptance matrix from `prd.md`.

## Validation Commands

```bash
bash -n scripts/apply-openvela-ui-performance-patch.sh
python3 -m py_compile harness/velaguard_ui_performance_check.py
python3 harness/velaguard_ui_performance_check.py --artifacts .debug
python3 harness/velaguard_issue1_check.py --artifacts .debug
python3 harness/velaguard_settings_check.py --artifacts .debug
python3 harness/qspi_boot_flow_check.py --artifacts .debug
```

Add a dedicated DMA2D harness before implementation and run NuttX checkpatch on
all new H7 driver sources. Hardware results must be recorded separately from
software/build checks.

## Implementation Status

- Complete: visible native monitor, 16 ms periods, fast-poll default, rollback
  switches and metadata, SDRAM reservation, asynchronous VBlank flip, RGB565
  DMA2D fill (+ buffer-copy) unit, 256-pixel fill threshold (aligned to design
  and harness), cache maintenance, timeout fallback, and serial counters.
- Software-verified (2026-07-21): `velaguard_dma2d_check.py --artifacts .debug`
  exit 0; also green: UI performance, ISSUE1, settings/network, QSPI boot-flow
  harnesses against rebuilt `.debug` production/release artifacts.
- Rebuilt (2026-07-21 21:46 +0800): WSL incremental
  `bash .debug/openvela_windows_build.sh` after MIN_PIXELS=256; ELF exports
  `vg_ui_dma2d_init` and DMA2D counters; `render_backend=dma2d`,
  `perf_monitor=visible`, `touch_mode=fast-poll`.
- Hardware-pending: flash via Windows Cube (`windows_flash_cube.ps1 -NoBuild`);
  follow `hardware-acceptance.md` for the PRD matrix. This WSL session has no
  Cube CLI / ST-LINK serial path.

## Rollback Points

- Disable the LVGL DMA2D draw unit while keeping the H7 driver compiled.
- Disable the H7 DMA2D driver and use the unchanged software draw unit.
- Restore synchronous pan display only if asynchronous ownership validation
  fails; do not weaken VSYNC safety.
- Hide the monitor or select interrupt touch independently when isolating load.
