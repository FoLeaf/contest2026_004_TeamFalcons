# STM32H7 LVGL DMA2D 50 FPS acceleration

## Goal

Make VelaGuard scrolling and dragging responsive on STM32H750B-DK by removing
the software-rendering and synchronous-page-flip bottlenecks, while preserving
the existing 480x272 RGB565 product UI, QSPI XIP, networking, NSH, and touch
coordinate correctness.

## Background

- The prior first-stage task improved scheduling and touch sampling, but user
  hardware feedback still reports poor scrolling and low LVGL frame rate.
- LTDC currently provides scanout and VSYNC-safe double buffering only. LVGL
  has one software draw unit and no DMA2D/GPU/Arm-2D backend.
- STM32H7 LTDC source references `CONFIG_STM32H7_DMA2D`, but the H7 DMA2D
  driver, Kconfig, register header, and build integration are missing.
- `FBIOPAN_DISPLAY` waits synchronously for vertical blank and blocks the UI
  thread. The fixed LTDC framebuffer also overlaps the SDRAM heap range.

## Requirements

- R1: Show LVGL's native FPS/CPU performance monitor at the top-left in every
  Debug, Release, and Production build. It must also report render, flush, and
  GPU timing; retain an emergency build switch to hide it.
- R2: Use `SCHED_CPULOAD_SYSCLK` for CPU load and schedule LVGL display and
  input handling every 16 ms.
- R3: Reserve the first 1 MiB of SDRAM for LTDC/DMA2D and start the SDRAM heap
  at `0xd0100000`; no allocator may own the fixed framebuffer range.
- R4: Preserve `/dev/fb0`, RGB565, direct rendering, LTDC double buffering,
  and VSYNC-safe flips. Page-flip submission must not block the LVGL thread.
- R5: Complete STM32H7 DMA2D support and register an LVGL v9 DMA2D draw unit.
  Accelerate supported fill, 1:1 blit, and blend tasks, with software fallback
  for unsupported formats/effects and runtime DMA2D errors.
- R6: Maintain cache coherency for all DMA read/write areas and never expose a
  partially rendered buffer to LTDC.
- R7: Use fast polling by default at the validated FT5X06 100 kHz bus rate:
  10 ms active and 20 ms idle. Retain interrupt mode as an explicit fallback.
- R8: Remove rounded-mask cost from repeated scrolling rows while retaining
  rounded static buttons/dialogs and the established industrial visual style.
- R9: Carry NuttX and apps/LVGL checkout changes through separate project-owned
  idempotent patches. Preserve existing dirty upstream checkout work.
- R10: Provide independent build rollback switches for DMA2D, the monitor,
  and interrupt touch, and record all selected modes in `build-info.txt`.

## Acceptance Criteria

- [ ] During a 60-second settings/network scrolling run after 5 seconds of
      warm-up, rendered and refreshed FPS stay at or above 50 and frame-time
      P95 is at most 20 ms.
- [ ] Scrolling CPU usage averages at most 85 percent and never remains at
      100 percent for one second; DMA2D builds report non-zero GPU time and
      hardware task count.
- [ ] DMA2D reference tests match software output for supported RGB565 fills,
      blits, alpha blends, clipped areas, and non-trivial strides.
- [ ] One hundred presses and thirty long drags contain no lost or stuck
      DOWN/MOVE/UP sequence; feedback latency P95 is at most 50 ms.
- [ ] No tearing, black frame, FIFO underrun, DMA timeout, cache artifact,
      framebuffer/heap overlap, or monotonically growing heap use occurs.
- [ ] The same performance run passes while Ethernet/DHCP, acquisition, and
      alarm services are active.
- [ ] `test/debug`, `test/release`, and `production/release` build and pass the
      VelaGuard, settings, QSPI, and new DMA2D static/artifact harnesses.
- [ ] `-DisableDma2d`, `-HidePerfMonitor`, and `-InterruptTouch` each produce a
      usable rollback image without changing the other two modes.

## Out of Scope

- A general-purpose GPU framework for non-STM32 targets.
- UI workflow redesign or a guaranteed 60 FPS product contract.
- Accelerating A8 masks, arbitrary transforms, gradients, or complex vector
  paths in the first DMA2D backend.
