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

**Minimal alarm (demo):** `vg_alarm_*` uses thresholds from
`/data/velaguard/configs/alarm.json` (auto-created defaults: warn 70 / crit 80 /
restore 68, 2 s). Home shows level color + last event line. Transitions go to
`latest.log` and `events.jsonl`. Edit the JSON on device to retune without rebuild.
Not the full issue #03/#05 product store.

Hardware pin contract for UART mode: `docs/velaguard-expansion-board.md`.

**中文 UI：** 首页使用子集字体 `src/fonts/vg_font_cn_16`（NotoSansSC 裁剪）。
新增中文文案前需把缺字补进字库后重新 `lv_font_conv`。

Still no networking, MQTT, AI, full alarm engine, or OTA.

Windows VS Code tasks build/flash with the external-QSPI workflow. Test mode uses
a compile-time Device ID override; production derives ID from the STM32 UID.
