# AI Bridge Diagnosis Loop

Status: incomplete

## Parent

.scratch/velaguard-independent-edge-ai-gateway/PRD.md

## What to build

Implement the first AI Bridge loop for alarm diagnosis. From an Active Alarm, VelaGuard should publish a structured AI diagnosis request over MQTT, the AI Bridge should call MiMo, and VelaGuard should show a structured diagnosis response or local fallback without blocking the Local Safety Loop.

## Acceptance criteria

- [ ] The alarm details page offers an AI diagnosis action for an Active Alarm.
- [ ] VelaGuard publishes an AI request containing `req_id`, Device ID, creation timestamp, request type, payload hash, alarm context, recent readings, and relevant Sensor Configuration.
- [ ] AI Bridge subscribes to the request topic and publishes responses to the matching device response topic.
- [ ] AI Bridge calls MiMo over HTTPS; MiMo credentials are not stored on the board.
- [ ] AI Bridge treats `req_id + payload_hash` as an idempotency key.
- [ ] Duplicate completed requests republish the same response.
- [ ] Duplicate in-progress requests produce a processing status rather than starting duplicate MiMo work.
- [ ] VelaGuard UI shows diagnosis states for processing, success, failure, and fallback.
- [ ] Successful diagnosis displays summary, risk level, possible causes, recommended actions, shutdown recommendation, and confidence.
- [ ] AI timeout or malformed response does not block Modbus acquisition, local alarms, or local UI.
- [ ] Diagnosis result or fallback is stored as a structured event/log.

## Blocked by

- .scratch/velaguard-independent-edge-ai-gateway/issues/03-local-safety-loop-with-modbus-sensor.md
- .scratch/velaguard-independent-edge-ai-gateway/issues/07-mqtt-identity-auth-status-and-topic-contract.md
- .scratch/velaguard-independent-edge-ai-gateway/issues/08-event-ids-time-quality-and-pending-event-resend.md
