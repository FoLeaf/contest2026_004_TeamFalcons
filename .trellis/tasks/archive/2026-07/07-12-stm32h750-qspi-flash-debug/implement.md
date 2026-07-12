# Implementation Plan

1. Restore and harden the historical QSPI support as team-owned assets.
   - Add the boot stub source, linker script, and deterministic build script.
   - Add an updated NuttX patch for QSPI linking, Kconfig selection, correct
     board Kconfig source condition, and executable MPU mapping.
   - Add an idempotent patch helper that validates the expected openvela root.
2. Build a generic Windows-to-WSL build wrapper.
   - Configure the stock `stm32h750b-dk:lvgl` target without requiring the
     removed VelaGuard app.
   - Enable QSPI boot and optional debug symbols with `kconfig-tweak`.
   - Copy the main and stub artifacts to a Windows-accessible output directory.
3. Build the CubeProgrammer flash wrapper.
   - Resolve tool settings from parameters/environment/default discovery.
   - Validate Intel HEX address ranges.
   - Program QSPI via External Loader, program the internal stub, then reset.
4. Add VSCode integration and documentation.
   - Add build/flash/debug tasks and an attach-only Cortex-Debug configuration.
   - Add Windows/WSL source-file mapping and configurable tool paths.
   - Document setup, workflow, failure recovery, and why OpenOCD is debug-only.
5. Add static and executable validation.
   - Check patch contents, linker addresses, script wiring, and debug no-load
     behavior with a repository harness.
   - Run `bash -n`, PowerShell parser checks when PowerShell is available,
     compile the boot stub, apply/check the patch, build the target when the
     workspace permits, and run the contest harness.
6. Review and rollback gates.
   - Inspect diffs before applying the patch and after generated parent-repo
     changes.
   - Do not overwrite unrelated dirty files; stop and report overlaps.
   - Record hardware validation as pending if Windows Cube tools/ST-LINK are
     unavailable.
7. Restore the FT5X06/LVGL touchscreen contract discovered during hardware
   validation.
   - Add `TSIOC_GETMAXPOINTS` handling to the maintained NuttX patch and the
     generated checkout.
   - Add a static regression assertion and run NuttX style/build checks.
   - Rebuild, program with CubeProgrammer, start `lvgldemo widgets`, and verify
   that LVGL opens the input device and receives touch interaction.
8. Replace the scaffold hello application with the team-owned VS Code Lab.
   - Add LVGL initialization, a single UI task, controls, heartbeat, serial
     diagnostics, and stable debug symbols.
   - Keep NSH as the foreground init shell after the UI task starts and guard
     against duplicate UI instances.
9. Wire the application into the Windows build.
   - Enable the team Kconfig option, select `vscode_lab_main` as init, and
     disable the stock `lvgldemo` application.
   - Extend the harness to verify application ownership, config wiring, and
     the debug contract.
10. Validate the complete experiment.
    - Build release/debug QSPI images, check address ranges and ELF symbols,
      program with CubeProgrammer, and confirm automatic UI startup.
    - Attach from VS Code and hit `vscode_lab_debug_checkpoint()` by touching
      the button while NSH remains available.
