# VelaGuard application

Bootable product slice for STM32H750B-DK: Device ID, `/data/velaguard` startup
logs, LVGL home/settings UI, Ethernet network status + DHCP/static apply, and
NSH-preserving entry.

## Acquisition backends

Kconfig `VG_ACQ_BACKEND`:

| Value | Meaning |
|-------|---------|
| **MOCK** (default) | Live demo temperature 40–85 °C, 1 Hz, no RS485 hardware |
| NONE | UI shows “Not configured” |
| UART | Reserved for issue #02 (`/dev/rs485` + nanoMODBUS); not implemented yet |

Home panel shows `采集/模拟` and the current value. Switch backend in menuconfig
or build kconfig-tweak when the expansion board arrives.

**Minimal alarm (demo):** `vg_alarm_*` uses thresholds from
`/data/velaguard/configs/alarm.json` (auto-created defaults: warn 70 / crit 80 /
restore 68, 2 s). Home shows level color + last event line. Transitions go to
`latest.log` and `events.jsonl`. Edit the JSON on device to retune without rebuild.
Not the full issue #03/#05 product store.

## Network settings (this slice)

- Home status bar shows **real** Ethernet summary (无链路 / 正在获取地址 / IPv4).
- Settings icon opens a category list; only **网络设置** is operable.
- Network page supports DHCP vs static IPv4 (address / netmask / gateway / DNS),
  full-screen numeric editor, confirm dialog, async apply, and rollback.
- Persistent file: `/data/velaguard/configs/network.json` (versioned, atomic write).
- Runtime policy owner: `netinit_set_ipv4_config()` (carrier recovery re-applies
  the same DHCP/static policy). UI never calls netlib/ioctl directly.
- Wi-Fi / ESP-01 is a **暂未开放** placeholder only.

Still no MQTT, AI Bridge, full alarm rule editor, audio config, or OTA.

## UI resources

- Icons: source SVG under `res/` (`setting.svg`, `settings_ethernet.svg`,
  `WIFI.svg`). Generated LVGL A8 descriptors live in `src/assets/`.
  Regenerate with `python3 scripts/gen_vg_icons.py`.
- Chinese fonts: subset NotoSansSC in `src/fonts/vg_font_cn_{16,20}.c`.
  Add missing glyphs with `lv_font_conv` before introducing new UI strings.

Hardware pin contract for UART mode: `docs/velaguard-expansion-board.md`.

Windows VS Code tasks build/flash with the external-QSPI workflow. Test mode uses
a compile-time Device ID override; production derives ID from the STM32 UID.
