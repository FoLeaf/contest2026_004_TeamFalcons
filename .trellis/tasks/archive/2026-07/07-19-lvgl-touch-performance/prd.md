# Optimize LVGL display and touch responsiveness

## Goal

Make the STM32H750B-DK VelaGuard UI responsive enough for repeated product
interaction while preserving the current 480x272 industrial UI, QSPI boot,
NSH rescue console, networking, and storage-degradation behavior.

Release firmware is the strict product-performance target. Debug firmware
must retain symbols and remain usable for day-to-day hardware debugging.

## Background

- The current artifact is `test/debug` with `CONFIG_DEBUG_NOOPT=y`.
- LVGL uses RGB565 direct rendering into the STM32H7 LTDC double framebuffer.
- `CONFIG_LV_DEF_REFR_PERIOD=33` caps refresh scheduling at about 30 Hz with
  no margin for rendering or VSYNC.
- FT5X06 currently uses adaptive polling: 50 ms while touched and up to
  200 ms while idle, before LVGL input scheduling is included.
- The board already implements the FT5X06 EXTI callbacks required by the
  interrupt-driven driver path.

## Requirements

- R1: Release builds must retain full optimization; debug builds must use
  `-Og` with debug symbols instead of `-O0`.
- R2: Schedule LVGL display refresh and input reads every 20 ms.
- R3: Prefer the FT5X06 interrupt path. If board validation finds missing or
  stuck down/move/up events, use deterministic 20 ms idle / 10 ms active
  polling as the fallback.
- R4: Run FT5X06 I2C4 at 400 kHz after validating reliable reads on hardware.
- R5: Keep RGB565, double framebuffer, direct rendering, and VSYNC-safe page
  flips. Do not trade tearing or event correctness for benchmark results.
- R6: Avoid invalidating LVGL objects when their displayed value and semantic
  style have not changed.
- R7: Provide opt-in, test-only performance diagnostics. Production builds
  must not show an FPS overlay or run a synthetic animation.
- R8: Carry NuttX/LVGL checkout changes through project-owned idempotent patch
  scripts; do not depend on uncommitted upstream checkout state.
- R9: Preserve existing screen navigation, touchscreen coordinate mapping,
  network behavior, startup behavior, NSH, and QSPI XIP contracts.

## Acceptance Criteria

- [ ] `test/release` sustains at least 30 FPS under the diagnostic animation;
      frame-time P95 is no more than 33.3 ms over a 60-second capture after a
      5-second warm-up.
- [ ] End-to-end press feedback over at least 100 physical presses has P95 no
      more than 50 ms in Release, with no missed press or release.
- [ ] Debug firmware contains `-g3`, uses `-Og`, remains attach-debuggable,
      and has physical press-feedback P95 no more than 100 ms.
- [ ] Twenty drag gestures and 50 home/settings/network/editor navigation
      cycles complete without stuck state, coordinate regression, tearing,
      black frames, or monotonically growing heap use.
- [ ] The Release metrics remain within target while Ethernet/DHCP,
      acquisition, and alarm service work are active.
- [ ] `test/debug`, `test/release`, and `production/release` build and pass the
      VelaGuard, settings, and QSPI artifact harnesses.
- [ ] Performance diagnostics are absent by default from production config
      and product-visible UI.
- [ ] Existing cold-boot, storage-degraded, touchscreen initialization, NSH,
      and QSPI validation cases still pass.

## Out of Scope

- UI visual redesign or new product workflows.
- A 60 FPS product guarantee.
- DMA2D enablement without profiling evidence that software rendering remains
  the limiting stage after the required changes.
