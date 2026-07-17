# Natural Language Candidate Configuration Flow

Status: incomplete

## Parent

.scratch/velaguard-independent-edge-ai-gateway/PRD.md

## What to build

Implement natural-language sensor setup as a safe Candidate Configuration flow. A user can describe a Modbus sensor, AI Bridge returns a structured candidate, VelaGuard validates and previews it, the operator runs a test read, and only Local Confirmation can make it active.

## Acceptance criteria

- [ ] The UI provides a natural-language add-sensor entry point.
- [ ] VelaGuard sends the natural-language prompt to AI Bridge over MQTT with request IDs and payload hash.
- [ ] AI Bridge returns a structured Candidate Configuration or a request for missing fields.
- [ ] VelaGuard rejects malformed JSON and unsupported schema versions.
- [ ] VelaGuard validates protocol, serial fields, slave address, function code, register address, data type, scale, poll interval, and alarm rules.
- [ ] Risk checks identify dangerous or unsupported changes before preview.
- [ ] Preview shows protocol, slave address, serial settings, registers, scale/unit, rules, and risk notes.
- [ ] Test read must succeed before activation.
- [ ] Failed test read can save as disabled draft but cannot become active.
- [ ] Local Confirmation is required before activation.
- [ ] Activated Sensor Configuration is persisted and starts normal acquisition.
- [ ] All state transitions write structured events.

## Blocked by

- .scratch/velaguard-independent-edge-ai-gateway/issues/02-nanomodbus-rs485-acquisition-path.md
- .scratch/velaguard-independent-edge-ai-gateway/issues/05-durable-config-store-and-schema-recovery.md
- .scratch/velaguard-independent-edge-ai-gateway/issues/07-mqtt-identity-auth-status-and-topic-contract.md
- .scratch/velaguard-independent-edge-ai-gateway/issues/09-ai-bridge-diagnosis-loop.md
