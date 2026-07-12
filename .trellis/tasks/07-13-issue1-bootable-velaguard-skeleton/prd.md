# Implement Issue 01 Bootable VelaGuard Skeleton

## Goal

Deliver the first hardware-bootable VelaGuard product slice on
STM32H750B-DK/openvela, replacing the development-only VS Code Lab as the
default firmware application while preserving the proven QSPI build, flash,
touch, cold-boot, and attach-debug workflow.

## Background

The authoritative product issue is
`.scratch/velaguard-independent-edge-ai-gateway/issues/01-bootable-velaguard-skeleton.md`.
The previous implementation on historical branch `5eb5070` established useful
module seams but did not close production Device ID validation, storage/log
failure visibility, or reproducible integration without a separate Issue 1
NuttX patch. Its five equal status cards also do not provide a strong visual
hierarchy on the 480x272 display.

The current branch has a hardware-validated QSPI-XIP boot stub, CubeProgrammer
External Loader flow, framebuffer/LVGL initialization, FT5X06 compatibility,
NSH-preserving app startup, Windows VS Code tasks, and source debugging. ISSUE1
must build on this baseline rather than reintroducing the old boot path.

## Requirements

- Add a project-owned VelaGuard application with separate identity, startup
  record, and LVGL home-screen modules.
- Start board services through `nsh_initialize()`, run the LVGL UI in its own
  task, and preserve `nsh_consolemain()` on the serial console.
- Support explicit test and production product modes independently from
  release/debug compiler optimization.
- Test mode accepts only a compile-time Device ID override.
- Production mode derives the Device ID from the STM32H750 96-bit UID and adds
  no runtime edit path.
- Create or verify `/data/velaguard`, `/data/velaguard/configs`, and
  `/data/velaguard/logs` during startup.
- Append one human-readable startup record and one JSONL boot event containing
  Device ID, build mode, firmware version, boot ID, `ts_ms`, `uptime_ms`, and
  time quality.
- Expose storage/event success or degradation on both serial output and the
  home screen.
- Display device identity, build mode, firmware version, and distinct status
  areas for acquisition, alarm, network, audio, and time.
- Preserve the current QSPI address boundaries, CubeProgrammer flash ownership,
  OpenOCD attach-only policy, framebuffer path, and touch-driver compatibility.
- Keep Modbus, real sensor acquisition, networking, MQTT, AI, audio playback,
  OTA, and runtime configuration outside this issue.
- Follow the user-requested `design-taste-frontend` audit and pre-flight rules
  where they apply to a fixed 480x272 industrial LVGL product surface.

## Visual Constraints

- The user selected an industrial monitoring-console direction rather than a
  brand showcase or developer diagnostics screen. Operational state has visual
  priority; debug detail stays on serial NSH and in GDB.
- One fixed dark industrial theme suitable for the embedded HMI; this is not a
  consumer web page and does not require a browser dark-mode toggle.
- One cyan product accent; red and amber are reserved for semantic alarm and
  warning states.
- Consistent 8 px panel radius, WCAG-like readable contrast, no decorative
  status dots, no five-equal-card template, and no gratuitous animation.
- English UI copy for the first slice because the enabled LVGL Montserrat fonts
  do not provide Chinese glyph coverage.
- Every required placeholder communicates an honest state such as awaiting
  source, no active alarm, offline, unavailable, or unsynced.

## Acceptance Criteria

- [x] Test and production QSPI images build from the team repository workflow.
- [ ] The test image shows the configured override; the production image shows
  a UID-derived stable Device ID with no runtime editor.
- [ ] A cold boot reaches the VelaGuard home screen without a connected PC.
- [ ] Repeated production cold boots show the same Device ID.
- [ ] The display shows VelaGuard, build mode, firmware version, Device ID,
  storage/event state, and all five required product status placeholders.
- [ ] Startup creates/verifies the three directories and writes both log files,
  or shows a clear degraded state if the filesystem cannot provide them.
- [ ] The structured boot event contains all required fields and valid JSONL.
- [ ] Serial NSH remains available and reports the same identity/startup state.
- [ ] Touch input initializes without breaking the non-interactive ISSUE1 home
  screen, and OpenOCD attach debugging loads symbols without programming QSPI.
- [ ] Static harnesses, release/debug builds, HEX range checks, cold boot, and
  source breakpoint validation pass.
- [ ] The Taste pre-flight applicable to an embedded industrial screen passes:
  hierarchy, color/shape consistency, copy audit, contrast, restrained motion,
  and absence of generic equal-card layout.

## Out of Scope

- Modbus acquisition and state machines from Issues 02-04.
- Durable configuration recovery, durable rolling logs, and storage mounting
  policy from Issues 05 and 13.
- Ethernet/Wi-Fi, MQTT, AI Bridge, Candidate Configuration, audio, and OTA.
- Changes to public openvela source trees as the ISSUE1 product implementation.
