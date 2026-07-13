# ISSUE1 Bootable VelaGuard Skeleton Design

## Boundaries

ISSUE1 creates the VelaGuard product process and its first observable boot
state. It owns identity, startup records, and the LVGL home screen. It does not
implement real acquisition, alarms, networking, audio, AI, configuration, or
OTA.

The implementation remains inside the team repository. A manifest link exposes
`app/velaguard_app` to openvela. The build helper may repair that generated
contest-owned link, but ISSUE1 adds no public-tree source patch. The existing
QSPI and FT5X06 compatibility patch remains a build-platform dependency and is
not extended for VelaGuard behavior.

## Process Architecture

```text
velaguard_main
  -> nsh_initialize()
  -> vg_identity_init()
  -> vg_startup_record()
  -> task_create(velaguard_ui)
  -> nsh_consolemain()

velaguard_ui
  -> lv_init()
  -> lv_nuttx_dsc_init()
  -> lv_nuttx_init()
  -> vg_ui_home_create()
  -> lv_timer_handler() loop
```

`nsh_initialize()` owns board initialization. Keeping `nsh_consolemain()` in
the entry task preserves COM7/115200 NSH while the UI runs independently. A UI
initialization failure never removes the rescue console.

## Modules and Contracts

### Identity

```c
enum vg_build_mode
{
  VG_BUILD_TEST,
  VG_BUILD_PRODUCTION
};

struct vg_identity
{
  char device_id[40];
  enum vg_build_mode build_mode;
  const char *firmware_version;
};

int vg_identity_init(void);
const struct vg_identity *vg_identity_get(void);
```

Test mode validates the compile-time override against `[A-Za-z0-9_-]`.
Production mode reads the STM32H750 96-bit UID and formats
`vg-<24 lowercase hex>`. No runtime setter or command exists.

### Startup record

```c
enum vg_time_quality
{
  VG_TIME_UNKNOWN,
  VG_TIME_RTC,
  VG_TIME_NTP,
  VG_TIME_CLOUD
};

struct vg_startup_state
{
  bool directories_ready;
  bool human_log_written;
  bool event_written;
  int storage_error;
  char boot_id[24];
  uint64_t ts_ms;
  uint64_t uptime_ms;
  enum vg_time_quality time_quality;
};

int vg_startup_record(void);
const struct vg_startup_state *vg_startup_get(void);
```

Startup creates `/data/velaguard`, `configs`, and `logs`, appends a readable
line to `latest.log`, then appends a JSON object to `events.jsonl`. Each step
records its own outcome. The boot continues when storage is unavailable, but
serial and UI state must say `DEGRADED`.

`boot_id` uses 64 random bits from `/dev/urandom` with a monotonic fallback.
`ts_ms` records `CLOCK_REALTIME`; `time_quality` is `rtc` only when the calendar
is credible, otherwise `unknown`. `uptime_ms` uses `CLOCK_MONOTONIC`.

### UI

The UI reads immutable identity/startup snapshots. It does not read UID
registers or perform filesystem I/O. A one-second LVGL timer updates uptime and
provides a stable debug breakpoint symbol.

## Build Matrix

Product mode and compiler mode are orthogonal:

| Product mode | Compiler mode | Purpose |
|---|---|---|
| test | release | normal development run |
| test | debug | source breakpoint and hardware inspection |
| production | release | ISSUE1 identity/cold-boot acceptance |
| production | debug | optional production-path diagnosis |

`windows_build_openvela.ps1` gains a validated VelaGuard mode parameter and
selects `CONFIG_LVX_USE_VELAGUARD`. It disables the VS Code Lab and stock LVGL
demo, sets `velaguard_main`, enables required fonts and pseudo-file support,
and applies the existing QSPI platform configuration. Flash remains owned by
CubeProgrammer. OpenOCD remains attach-only.

## UI Design

Design Read: a 480x272 industrial device home screen for on-site operators,
with a trust-first monitoring-console language implemented in native LVGL.

- `DESIGN_VARIANCE: 4`
- `MOTION_INTENSITY: 2`
- `VISUAL_DENSITY: 7`

```text
+----------------------------------------------------------+
| VelaGuard                         TEST      FW 0.1.0      |
+----------------------------------------------------------+
| LOCAL GATEWAY                                            |
| vg-test-001                           Storage: READY      |
| Boot 7F2A91C4                         Uptime 00:00:12      |
+------------------------------------+---------------------+
| ACQUISITION                        | NETWORK             |
| Not configured                     | Offline             |
|                                    +---------------------+
| ALARM                              | AUDIO               |
| Not armed                          | Unavailable         |
|                                    +---------------------+
|                                    | TIME                |
|                                    | Unsynced            |
+------------------------------------+---------------------+
```

Tokens:

| Role | Value |
|---|---|
| background | `#0C1218` |
| surface | `#151E26` |
| raised surface | `#1C2731` |
| primary text | `#E7EDF2` |
| muted text | `#8D9AA5` |
| product accent | `#39B6B2` |
| semantic alarm | `#E05B5B` |
| semantic warning | `#D7A84A` |

All panels use an 8 px radius. Cyan is the only product accent. Red and amber
appear only for actual semantic states. There are no decorative dots, glows,
gradients, five equal cards, fake controls, or automatic animation. English
copy avoids missing glyphs in the enabled Montserrat fonts.

## Error Behavior

| Failure | Serial | UI | Process |
|---|---|---|---|
| invalid test Device ID | explicit identity error | `IDENTITY ERROR` | rescue NSH stays available |
| directory creation fails | path and errno | `Storage: DEGRADED` | UI still starts |
| readable log fails | file and errno | `Storage: DEGRADED` | JSONL still attempted |
| JSONL event fails | file and errno | `Storage: DEGRADED` | UI still starts |
| LVGL display fails | explicit display error | unavailable | UI task exits, NSH remains |
| touch input fails | touch degradation | non-interactive home still shown | display loop continues |

## Compatibility and Rollback

The VS Code Lab remains in `app/hello_app` as a reference application but is
disabled in VelaGuard builds. Reverting the VelaGuard app, manifest link, task
selection, and harness additions restores the current Lab workflow without
changing the QSPI boot stub or flash addresses.

## Validation

Static checks validate source contracts, UI tokens/copy, manifest linkage,
PowerShell task selection, JSON files, and prohibited layout patterns. Builds
validate test and production configuration, required symbols, debug sections,
and QSPI/internal HEX ranges. Hardware validation covers test-debug display,
NSH, touch initialization, source attach, production Device ID, and two cold
boots with an identical production identity.
