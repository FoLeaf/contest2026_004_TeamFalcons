# MQTT-only Pull-based OTA

Status: incomplete

## Parent

.scratch/velaguard-independent-edge-ai-gateway/PRD.md

## What to build

Implement MQTT-only OTA using pull-based firmware chunks over the existing MQTT/TLS connection. VelaGuard should receive an OTA Offer, require safe local acceptance, pull chunks with backpressure, write a Staging Image, verify hash and signature, switch only when safe, confirm after self-test, and roll back on failure.

## Acceptance criteria

- [ ] VelaGuard receives OTA Offers over the accepted MQTT topic contract.
- [ ] OTA Offer displays version, size, compatibility, risk notes, and signature metadata in the UI.
- [ ] OTA requires Local Confirmation or an accepted maintenance-window policy before downloading.
- [ ] OTA is blocked during high-severity Active Alarms or unsafe local storage/power conditions.
- [ ] VelaGuard requests firmware chunks instead of accepting uncontrolled cloud push.
- [ ] Chunk size and inflight count are bounded so OTA does not starve Modbus acquisition or UI responsiveness.
- [ ] Chunks are written to a Staging Image, not over the running image.
- [ ] Completed image is verified by sha256 and digital signature.
- [ ] Invalid hash or signature rejects the update and reports failure.
- [ ] Boot switch occurs only after image verification.
- [ ] New firmware must mark itself confirmed after self-test.
- [ ] Failed self-test or failed confirmation rolls back to the previous firmware.
- [ ] OTA progress and result events are published and logged.

## Blocked by

- .scratch/velaguard-independent-edge-ai-gateway/issues/05-durable-config-store-and-schema-recovery.md
- .scratch/velaguard-independent-edge-ai-gateway/issues/07-mqtt-identity-auth-status-and-topic-contract.md
- .scratch/velaguard-independent-edge-ai-gateway/issues/08-event-ids-time-quality-and-pending-event-resend.md
- .scratch/velaguard-independent-edge-ai-gateway/issues/12-ui-safety-and-remote-candidate-review.md
