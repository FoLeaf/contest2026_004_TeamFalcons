# Modbus Offline and Recovering State Model

Status: incomplete

## Parent

.scratch/velaguard-independent-edge-ai-gateway/PRD.md

## What to build

Implement the Modbus device communication state model around the existing acquisition path. VelaGuard should distinguish online, degraded, offline, and recovering, preserve prior alarm state while offline, probe at low frequency, and require consecutive successful reads before returning online.

## Acceptance criteria

- [ ] Modbus communication state includes `online`, `degraded`, `offline`, and `recovering`.
- [ ] Read timeouts, CRC errors, and no-response cases move the device through degraded toward offline according to configured thresholds.
- [ ] Offline state keeps the last known value visibly stale instead of deleting or silently clearing previous sensor alarms.
- [ ] Offline state uses low-frequency probe reads instead of normal polling.
- [ ] Recovering state resumes full reads but requires configured consecutive successes before returning online.
- [ ] UI distinguishes fresh, stale, offline, and recovering states.
- [ ] Offline/recovering transitions write structured events.
- [ ] Modbus state changes do not block local rule processing for other active devices or UI responsiveness.

## Blocked by

- .scratch/velaguard-independent-edge-ai-gateway/issues/02-nanomodbus-rs485-acquisition-path.md
