# Competition Demo Hardening

Status: incomplete

## Parent

.scratch/velaguard-independent-edge-ai-gateway/PRD.md

## What to build

Package the implemented VelaGuard slices into a repeatable competition demo. The demo should show independent H750B-DK operation, openvela UI/network/filesystem/audio/task value, Modbus acquisition, local alarms, MQTT AI diagnosis, safe configuration, logs, and network-failure degradation.

## Acceptance criteria

- [ ] A five-minute demo script exists and follows the accepted VelaGuard storyline.
- [ ] Demo starts from H750B-DK boot and does not require a long-running PC sidecar.
- [ ] Demo shows LVGL home/status UI, live Modbus reading, and alarm details.
- [ ] Demo injects or simulates a threshold alarm and shows UI plus local audio alarm.
- [ ] Demo requests AI diagnosis over MQTT and displays structured diagnosis.
- [ ] Demo shows natural-language Candidate Configuration or an already configured sensor flow with Local Confirmation.
- [ ] Demo shows logs or structured events proving acquisition, alarms, AI request/response, and confirmations.
- [ ] Demo includes a network-failure segment where local acquisition and alarms continue.
- [ ] README, architecture diagram, wiring notes, MQTT topic summary, and AI Bridge summary are prepared.
- [ ] The demo can run repeatedly without manual hidden recovery steps.
- [ ] Known limitations and out-of-scope items are documented honestly.

## Blocked by

- .scratch/velaguard-independent-edge-ai-gateway/issues/02-nanomodbus-rs485-acquisition-path.md
- .scratch/velaguard-independent-edge-ai-gateway/issues/03-local-safety-loop-with-modbus-sensor.md
- .scratch/velaguard-independent-edge-ai-gateway/issues/06-rj45-first-network-manager-with-esp01-fallback.md
- .scratch/velaguard-independent-edge-ai-gateway/issues/09-ai-bridge-diagnosis-loop.md
- .scratch/velaguard-independent-edge-ai-gateway/issues/10-natural-language-candidate-configuration-flow.md
- .scratch/velaguard-independent-edge-ai-gateway/issues/13-minecraft-like-rolling-logger-and-log-viewer.md
