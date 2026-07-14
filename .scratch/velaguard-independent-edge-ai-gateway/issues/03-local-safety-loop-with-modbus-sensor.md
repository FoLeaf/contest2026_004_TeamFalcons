# Local Safety Loop With Modbus Sensor

Status: incomplete

## Parent

.scratch/velaguard-independent-edge-ai-gateway/PRD.md

## What to build

Turn the Modbus acquisition path into the Local Safety Loop. VelaGuard should evaluate local rules from real Modbus readings, create Active Alarms, show the primary alarm on the home screen, list alarms on a details page, play local alarm audio, and persist alarm/log events without any network dependency.

## Acceptance criteria

- [ ] A Modbus reading can trigger a threshold alarm after `trigger_duration_ms`.
- [ ] A threshold alarm resolves only after the safe condition persists for `restore_duration_ms`.
- [ ] A jump alarm can trigger from a configured window delta and duration.
- [ ] Multiple Active Alarms can exist at the same time.
- [ ] The home screen shows the highest-priority current status and alarm count.
- [ ] The alarm details page lists all Active Alarms with current value, threshold/rule, first seen, last seen, acknowledgement, and resolution state.
- [ ] Acknowledgement records operator awareness without resolving the alarm.
- [ ] The same unresolved alarm updates the same `alarm_id` instead of creating duplicates.
- [ ] Local alarm sound plays for configured severities, and mute does not disable visual alarms.
- [ ] The Local Safety Loop continues to run with Ethernet unplugged, ESP-01 unavailable, MQTT disconnected, and AI Bridge unavailable.
- [ ] Optional expansion-board **DO1 (D6 low-side)** may drive a demo load on alarm only if firmware policy allows; **AI must not drive DO without Local Confirmation** (see `docs/velaguard-expansion-board.md` §3.3). DO is not required for this issue’s core loop.

## Hardware notes

- Modbus path remains UART7 / `/dev/rs485` per issue #02 and expansion-board contract; network bearer choice must not starve acquisition.
- **Without hardware:** `app/velaguard_app` already has a **demo** threshold loop on mock acquisition (`vg_alarm_*`, warn 70 / crit 80 °C). This issue still requires multi-alarm list, ack, audio, and durable events — treat demo as a visual smoke path only.

## Blocked by

- .scratch/velaguard-independent-edge-ai-gateway/issues/02-nanomodbus-rs485-acquisition-path.md
