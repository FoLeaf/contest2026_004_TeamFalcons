# Event IDs, Time Quality, and Pending Event Resend

Status: incomplete

## Parent

.scratch/velaguard-independent-edge-ai-gateway/PRD.md

## What to build

Make local events and MQTT delivery robust across reboot, reconnect, and duplicate delivery. VelaGuard should generate stable request, event, and alarm identifiers, record timestamp quality, persist critical Pending Events, and resend them after MQTT recovery without rewriting historical event times.

## Acceptance criteria

- [ ] Each boot has a `boot_id` recorded in logs and structured events.
- [ ] `req_id`, `event_id`, and `alarm_id` are generated consistently using Device ID, boot identity, and sequence information where appropriate.
- [ ] Critical event sequence state survives reboot enough to avoid confusing duplicate event IDs.
- [ ] Events include `ts_ms`, `uptime_ms`, and `time_quality`.
- [ ] Supported time qualities include `unknown`, `rtc`, `ntp`, and `cloud`.
- [ ] Events created before reliable wall time remain valid and ordered by uptime.
- [ ] Network recovery does not rewrite historical `ts_ms`.
- [ ] Cloud-bound critical events are stored as Pending Events when MQTT is unavailable.
- [ ] Pending Events are resent after MQTT reconnect with duplicate-safe identifiers.
- [ ] Corrupt Pending Event records are skipped without blocking startup.

## Blocked by

- .scratch/velaguard-independent-edge-ai-gateway/issues/05-durable-config-store-and-schema-recovery.md
- .scratch/velaguard-independent-edge-ai-gateway/issues/07-mqtt-identity-auth-status-and-topic-contract.md
