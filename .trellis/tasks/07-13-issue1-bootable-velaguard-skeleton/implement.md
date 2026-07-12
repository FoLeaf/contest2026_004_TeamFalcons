# ISSUE1 Implementation Plan

## Implementation Checklist

1. Add `app/velaguard_app` build metadata and Kconfig.
2. Implement `vg_identity` with validated test override and production UID.
3. Implement `vg_startup` directory, readable log, JSONL event, timestamps,
   boot ID, and independent degradation flags.
4. Implement `vg_ui_home` with the approved Taste tokens, asymmetric status
   hierarchy, uptime refresh, and stable breakpoint symbol.
5. Implement `velaguard_main` with NSH-owned board initialization, a separate
   LVGL task, framebuffer path preservation, touch degradation, and serial NSH.
6. Add the manifest link and a guarded helper for generated openvela linkage.
7. Extend Windows build/flash tasks for VelaGuard test and production modes
   while retaining release/debug separation and OpenOCD attach-only behavior.
8. Add `harness/velaguard_issue1_check.py` and extend QSPI flow coverage.
9. Update ISSUE1 build/run documentation without pulling later issues into the
   slice.

## Software Validation

```bash
python3 -m py_compile \
  harness/qspi_boot_flow_check.py \
  harness/velaguard_issue1_check.py
python3 harness/velaguard_issue1_check.py
python3 harness/qspi_boot_flow_check.py --artifacts .debug
git diff --check
```

Build at minimum:

```text
test + debug
test + release
production + release
```

For each build, inspect `.config`, ELF symbols, section/debug metadata, and HEX
address ranges. Run Windows `-ValidateOnly` after each generated artifact set.

## Hardware Validation

1. Flash test-debug with CubeProgrammer and the External Loader.
2. Confirm cold boot reaches the approved VelaGuard screen.
3. Confirm serial shows identity, startup directories/event, UI readiness, and
   NSH prompt.
4. Confirm touch input initializes without preventing the home screen.
5. Attach OpenOCD without loading and hit the uptime breakpoint.
6. Flash production-release and record Device ID from UI and serial.
7. Disconnect all USB power, reconnect, and record Device ID again.
8. Require both production values to match exactly.

## Taste Pre-flight

- Confirm the declared Design Read and dial values remain accurate.
- Confirm one dark theme, one cyan accent, and semantic-only red/amber.
- Confirm the 8 px radius rule and readable text contrast.
- Confirm no equal five-card row, decorative dots, gradient, glow, fake
  controls, or meaningless motion.
- Re-read every visible string and verify honest placeholder states.
- Inspect the physical screen for clipping at 480x272.

## Risk and Rollback Points

- If generated link maintenance affects non-contest paths beyond the declared
  symlink, stop and use manifest/repo sync only.
- If production UID access faults, compare against STM32H750 UID constants and
  the arch implementation before changing the identity format.
- If storage files cannot be created with pseudo-file support, retain visible
  degradation and do not silently claim success; durable mounting stays out of
  ISSUE1.
- If LVGL display regresses, preserve `lv_nuttx_dsc_init()` framebuffer choice
  and compare with the working VS Code Lab before changing board drivers.
- The rollback is to disable VelaGuard and reselect the untouched VS Code Lab;
  never alter QSPI addresses to work around an application failure.

## Validation Snapshot (2026-07-13)

- `test/debug`, `test/release`, and `production/release` build successfully with
  the complete openvela Python Kconfig tool path.
- Both ISSUE1 and QSPI harnesses pass for all retained variants.
- Windows PowerShell `-DebugBuild` reaches the child build correctly, and Cube
  `-NoBuild -ValidateOnly` finds the External Loader and accepts both HEX
  ranges without accessing hardware.
- The final `.debug` hand-off contains `test/debug` symbols and is ready for
  Cube programming plus OpenOCD attach.
- Hardware acceptance remains pending because ST-LINK was not enumerated by
  Windows or WSL at the final flash attempt. No programming had started when
  Cube reported `No debug probe detected`.
