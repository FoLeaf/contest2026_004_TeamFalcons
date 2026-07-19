# LVGL and Touch Performance Design

## Architecture

The existing LVGL NuttX framebuffer backend remains the display boundary. It
already maps two LTDC RGB565 framebuffers, renders in direct mode, and flips
with `FBIOPAN_DISPLAY`. The optimization changes scheduling, compilation, and
input delivery before considering a display backend fork.

The FT5X06 driver should deliver samples through its existing board EXTI and
HPWORK path. The team repository owns a small idempotent openvela patch for
the I2C frequency or any required input-driver correction. Build configuration
selects interrupt mode. A compile-time fast-poll fallback is retained only if
hardware validation proves the board interrupt unreliable.

## Data Flow

```text
FT5X06 INT -> HPWORK -> I2C4 sample -> /dev/input0
           -> LVGL 20 ms input timer -> widget event -> invalidated area
           -> software RGB565 direct draw -> LTDC back buffer -> VSYNC flip
```

The UI service timer remains at one second. Home/network fields cache their
last rendered text and color so unchanged service polls do not invalidate the
display.

## Build and Diagnostics

- Release: `CONFIG_DEBUG_FULLOPT=y`.
- Debug: symbols `-g3`, `CONFIG_DEBUG_CUSTOMOPT=y`, `-Og`.
- Common product builds: `LV_DEF_REFR_PERIOD=20`, UI priority 120, interrupt
  FT5X06, 400 kHz I2C.
- An app Kconfig option enables a compact diagnostic animation and serial
  summary. It defaults off and is never automatically enabled for production.
- Windows build/flash entry points expose a named `-UiPerfDiagnostics` switch
  and record it in `build-info.txt`.

## Compatibility and Rollback

The framebuffer path stays owned by `lv_nuttx_dsc_init()` and remains
`/dev/fb0`. RGB565, double buffering, coordinate swap, single-point mode, and
`TSIOC_GETMAXPOINTS` remain unchanged.

Rollback is configuration-based: disable diagnostics; restore 33 ms refresh;
or re-enable the deterministic fast-poll fallback. Every upstream patch must
pass both forward and reverse `git apply --check` so repeated builds are safe.

DMA2D or LVGL framebuffer changes are a gated follow-up only when captured
render/flush timings show frame-time P95 above 33.3 ms after this design is in
place. They are not part of the default implementation.
