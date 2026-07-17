# Bootable VelaGuard Skeleton

Status: incomplete

## Parent

.scratch/velaguard-independent-edge-ai-gateway/PRD.md

## What to build

Build the first bootable VelaGuard application skeleton on STM32H750B-DK/openvela. The slice should boot without a connected PC, show a basic LVGL home screen, expose the current build mode and Device ID, initialize the basic local data directory, and write a startup log/event. This establishes the product boundary before Modbus, networking, AI, or OTA are added.

## Acceptance criteria

- [ ] VelaGuard boots on the target board or supported board configuration without requiring a long-running PC sidecar.
- [ ] LVGL displays a VelaGuard home screen with device identity, build mode, firmware version, and placeholder status areas for acquisition, alarm, network, audio, and time.
- [ ] Test build mode can override Device ID through compile-time/code configuration.
- [ ] Production build mode derives Device ID from STM32 UID and exposes no runtime Device ID edit path in the UI or debug command surface added by this slice.
- [ ] A local VelaGuard data directory is created or verified on startup.
- [ ] Startup writes a human-readable log entry and one structured event containing Device ID, build mode, firmware version, boot ID, uptime, and time quality.
- [ ] The system can restart cleanly and show the same production Device ID.

## Blocked by

None - can start immediately
