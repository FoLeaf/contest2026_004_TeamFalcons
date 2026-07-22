# Hardware acceptance — DMA2D 50 FPS

**Date prepared**: 2026-07-21  
**Image**: `.debug/` after incremental rebuild (MIN_PIXELS=256)

## Artifact identity

| Field | Value |
|---|---|
| product_mode | production |
| compiler_mode | release |
| render_backend | dma2d |
| perf_monitor | visible |
| touch_mode | fast-poll |
| ui_perf | disabled |
| CONFIG_VG_STM32H7_DMA2D | y |
| CONFIG_LV_DEF_REFR_PERIOD | 16 |
| CONFIG_LV_USE_PERF_MONITOR | y (top-left) |
| CONFIG_FT5X06_POLLMODE | y |
| ELF symbols | `vg_ui_dma2d_init`, counters, evaluate/dispatch present |
| text size | ~498632 |

## Flash (Windows, no rebuild)

From the contest repo on Windows (PowerShell), after ST-LINK + CubeProgrammer are available:

```powershell
.\scripts\windows_flash_cube.ps1 -NoBuild -VelaGuardMode production
```

Optional validation-only preflight:

```powershell
.\scripts\windows_flash_cube.ps1 -NoBuild -ValidateOnly
```

Open serial console (ST-LINK VCP, typically COM*) at 115200 after reset.

## Pass criteria (from prd.md)

Record PASS/FAIL with numbers.

### A. Boot / backend active

- [ ] Boot log contains `[velaguard][dma2d] RGB565 fill and buffer-copy acceleration enabled`
- [ ] Every ~5 s during UI activity: `[velaguard][dma2d] fill=… fallback=… errors=…` with **non-zero fill** and **errors not climbing**
- [ ] Top-left native FPS/CPU monitor visible

### B. 60 s scroll (settings/network list)

Warm-up 5 s, then 60 s continuous scroll/drag.

| Metric | Target | Measured |
|---|---|---|
| Rendered FPS | ≥ 50 | |
| Refreshed FPS | ≥ 50 | |
| Frame-time P95 | ≤ 20 ms | |
| Avg CPU while scrolling | ≤ 85 % | |
| CPU stuck at 100 % ≥ 1 s | must not happen | |
| DMA2D fill counter delta | > 0 | |
| Tear / black frame / FIFO underrun | none | |

### C. Touch

| Metric | Target | Measured |
|---|---|---|
| 100 presses DOWN/UP | no lost/stuck | |
| 30 long drags | no stuck MOVE | |
| Feedback latency P95 | ≤ 50 ms | |

### D. Stability under load

- [ ] Ethernet/DHCP active during the same scroll run
- [ ] Acquisition + alarm services active (or mock if no shield)
- [ ] Heap not monotonically growing (compare free heap before/after 60 s)
- [ ] No cache artifacts (static garbage bands)

### E. Rollback smoke (optional isolation)

| Image | Command | Result |
|---|---|---|
| software draw | flash with `-DisableDma2d` | boots, UI usable, fill counters absent/zero |
| hidden monitor | `-HidePerfMonitor` | overlay gone |
| interrupt touch | `-InterruptTouch` | touch still works |

## Serial capture tips

```text
# Look for:
[velaguard][dma2d] RGB565 fill and buffer-copy acceleration enabled
[velaguard][dma2d] fill=<n> fill_px=<n> copy=<n> ... fallback=<n> errors=<n> ...
```

Paste the 5 s counter lines from the start and end of the 60 s scroll window into `validation.md` when done.
