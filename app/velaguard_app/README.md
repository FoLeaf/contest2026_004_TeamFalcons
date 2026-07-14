# VelaGuard application

Bootable product slice for STM32H750B-DK: Device ID, `/data/velaguard` startup
logs, LVGL home screen, and NSH-preserving entry.

## Acquisition backends

Kconfig `VG_ACQ_BACKEND`:

| Value | Meaning |
|-------|---------|
| **MOCK** (default) | Live demo temperature 40–85 °C, 1 Hz, no RS485 hardware |
| NONE | UI shows “Not configured” |
| UART | Reserved for issue #02 (`/dev/rs485` + nanoMODBUS); not implemented yet |

Home panel shows `ACQ [MOCK]` and the current value. Switch backend in menuconfig
or build kconfig-tweak when the expansion board arrives.

**Minimal alarm (demo):** `vg_alarm_*` evaluates mock temperature with
warn ≥70 °C / crit ≥80 °C for 2 s, restore &lt;68 °C for 2 s. Home `ALARM` line
turns yellow/red. Level changes append to `/data/velaguard/logs/latest.log` and
`events.jsonl` via `vg_log_*`. Not the full issue #03 multi-alarm/ack UI.

Hardware pin contract for UART mode: `docs/velaguard-expansion-board.md`.

Still no networking, MQTT, AI, full alarm engine, or OTA.

Windows VS Code tasks build/flash with the external-QSPI workflow. Test mode uses
a compile-time Device ID override; production derives ID from the STM32 UID.
