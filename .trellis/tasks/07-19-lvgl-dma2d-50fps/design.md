# Design

## Architecture

Keep LTDC as the scanout engine and add DMA2D as an LVGL draw unit, not as a
replacement framebuffer backend. The software draw unit remains registered
for unsupported tasks and fault fallback.

```text
FT5X06 fast poll -> LVGL 16 ms input/refresh
  -> DMA2D draw unit (supported tasks) / software draw unit (fallback)
  -> RGB565 back buffer -> asynchronous VBR reload -> LTDC reload ISR
  -> framebuffer POLLOUT releases the next render
```

## Memory and Frame Scheduling

- Reserve `0xd0000000..0xd00fffff` from the SDRAM heap.
- Keep the two 480x272 RGB565 LTDC buffers at `0xd0000000` with total size
  522240 bytes. Place one 256 KiB DMA2D workspace at `0xd0080000` and begin
  the general SDRAM heap at `0xd0100000`.
- Change `stm32_pandisplay()` to submit `LTDC_SRCR_VBR` and return. A pending
  flag prevents a second reload; the reload ISR clears it and calls
  `fb_remove_paninfo()`. The LVGL fbdev `POLLOUT` gate prevents buffer reuse.

## DMA2D Boundary

- The implemented first stage is an application-owned STM32H750 draw unit.
  This avoids carrying an incomplete F7 lower-half port into the shared NuttX
  checkout while still exercising the H750 DMA2D engine from LVGL v9.
- Prefer DMA2D only for opaque, rectangular, non-gradient RGB565 fills of at
  least 256 pixels. A8 images, rounded corners, gradients, transforms, image
  blits, blends, and smaller tasks retain the software draw unit.
- Add blit/blend modes only after hardware counters show that fill offload is
  insufficient for the scrolling target and reference-output tests exist.
- DMA completion is interrupt-driven with a bounded wait. Timeout/error resets
  DMA2D and executes the task through software before marking it ready.

## Cache and Diagnostics

- Flush DMA source and any read-modify-write destination area before start;
  invalidate the cache-line-aligned destination area after completion.
- Register a draw-unit event callback so native LVGL sysmon receives GPU run
  time. Serial output reports hardware/fallback/error counters every five
  seconds so board testing can prove that the configured backend is active.
- Always enable the visible top-left native monitor. `-UiPerfDiagnostics`
  remains a separate synthetic-load switch; `-HidePerfMonitor` only hides the
  native overlay for rollback measurements.

## Compatibility and Rollback

- New PowerShell switches: `-DisableDma2d`, `-HidePerfMonitor`, and
  `-InterruptTouch`. Keep accepting `-FastTouchPoll` for compatibility even
  though fast-poll is now the default.
- `build-info.txt` records `render_backend=dma2d|software`,
  `perf_monitor=visible|hidden`, and `touch_mode=fast-poll|interrupt`.
- NuttX and apps patches apply independently, accept semantically complete
  later-overlapping states, and stop on partial application.

## Risks

- F7 and H7 DMA2D register layouts must be checked against H750 definitions;
  only verified modes are enabled.
- Asynchronous VBR reload must retain pan-buffer ownership until the ISR.
- Cache operations are part of correctness, not optional performance tuning.
