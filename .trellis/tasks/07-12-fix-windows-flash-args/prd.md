# Fix Windows PowerShell debug flash argument forwarding

## Goal

Make the Windows VS Code debug-flash task pass named options to child
PowerShell scripts without those options being rebound as positional values.

## Background

Running `windows_flash_cube.ps1 -DebugBuild` currently invokes
`windows_build_openvela.ps1` through an array of strings. PowerShell treats the
array elements as positional arguments, producing this observed binding:

- `WslDistro = "-DebugBuild"`
- `OpenvelaDir = "-OutDir"`

The build then fails with `WSL_E_DISTRO_NOT_FOUND`. The OpenOCD compatibility
wrapper uses the same forwarding pattern and has the same latent defect.

## Requirements

- Replace string-array parameter forwarding with named hashtable splatting in
  `scripts/windows_flash_cube.ps1`.
- Apply the same correction to `scripts/windows_flash_openocd.ps1`.
- Preserve existing defaults, switches, output paths, task labels, QSPI
  validation, programming order, and OpenOCD's debug-only policy.
- Keep the change limited to argument forwarding and regression coverage.
- The fix must work for both release and `-DebugBuild` invocations and for
  optional explicit Cube CLI, External Loader, output directory, no-build, and
  validate-only arguments.

## Acceptance Criteria

- [ ] `windows_flash_cube.ps1 -DebugBuild` forwards `DebugBuild=$true` and the
  real `.debug` output path by parameter name.
- [ ] A normal flash invocation still performs a release build when
  `-DebugBuild` is absent.
- [ ] `windows_flash_openocd.ps1` forwards every supported option by name to
  `windows_flash_cube.ps1`.
- [ ] The QSPI boot-flow harness detects regression to string-array splatting.
- [ ] The existing QSPI boot-flow harness, contest harness, and
  `git diff --check` pass.
- [ ] A Windows VS Code `openvela: flash debug firmware (CubeProgrammer)` run
  reports `Building debug QSPI-XIP firmware in WSL distro 'Debian'` rather
  than treating `-DebugBuild` as the distro name.

## Out of Scope

- Changing WSL installation or distribution names.
- Changing CubeProgrammer, External Loader, OpenOCD, or ST-LINK behavior.
- Refactoring the build or flash pipeline beyond the two forwarding sites.

## Notes

- Keep `prd.md` focused on requirements, constraints, and acceptance criteria.
- Lightweight tasks can remain PRD-only.
- For complex tasks, add `design.md` for technical design and `implement.md` for execution planning before `task.py start`.
