# Implementation Plan

1. Add static checks for the intended optimization level, 20 ms LVGL period,
   interrupt/fallback touch configuration, diagnostic default, and idempotent
   upstream patch application.
2. Update the Windows build and flash parameter chain with the named
   `UiPerfDiagnostics` switch; select `-Og` debug optimization and persist the
   selected performance metadata.
3. Add the opt-in VelaGuard diagnostic module/Kconfig integration and serial
   FPS/frame-time summary without changing the normal product UI.
4. Update common firmware configuration to 20 ms LVGL scheduling and UI
   priority 120.
5. Add/apply the scoped NuttX FT5X06 performance patch, select interrupt mode,
   and raise I2C4 touch frequency to 400 kHz.
6. Add change-detection around periodic home/network label and semantic-color
   updates that currently invalidate unchanged objects.
7. Run syntax/static harnesses and build `test/debug`; inspect `.config`, ELF,
   build metadata, patch idempotence, and existing artifact contracts.
8. Build `test/release` and `production/release`, then execute physical FPS,
   press, drag, page-cycle, network-load, cold-boot, and NSH validation.
9. If FT5X06 interrupt validation fails, replace it with the specified
   20 ms idle / 10 ms active poll timing and repeat the entire touch matrix.
10. If Release frame-time P95 still exceeds 33.3 ms, capture render versus
    VSYNC/flush timings before opening a separate DMA2D/backend optimization.

## Validation Commands

```bash
bash -n scripts/apply-openvela-ui-performance-patch.sh
python3 -m py_compile harness/velaguard_ui_performance_check.py
python3 harness/velaguard_ui_performance_check.py --artifacts .debug
python3 harness/velaguard_issue1_check.py --artifacts .debug
python3 harness/velaguard_settings_check.py --artifacts .debug
python3 harness/qspi_boot_flow_check.py --artifacts .debug
```

Build all retained variants through `scripts/windows_build_openvela.ps1` and
run CubeProgrammer `-NoBuild -ValidateOnly` before physical flashing.

## Risk and Rollback Points

- Preserve the user's existing NuttX Ethernet, netinit, Trellis, log, and asset
  changes; never regenerate or revert unrelated files.
- Commit the team-owned patch and apply helper, not incidental dirty state in
  the parent openvela checkouts.
- Treat missing hardware measurements as pending, never as a passed target.
- Revert each optimization independently through Kconfig/build switches if it
  regresses boot, input correctness, or display integrity.
