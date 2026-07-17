# MQTT Identity, Auth, Status, and Topic Contract

Status: incomplete

## Parent

.scratch/velaguard-independent-edge-ai-gateway/PRD.md

## What to build

Connect VelaGuard to the MQTT Broker using the accepted identity, authentication, topic, status, QoS, retained, and LWT rules. This slice establishes the cloud communication contract before AI, Pending Events, OTA, or voice chunks use it.

## Acceptance criteria

- [ ] VelaGuard connects to the MQTT Broker over the active network bearer.
- [ ] Formal/production builds use MQTTS with per-device token credentials.
- [ ] Test builds may use local plain MQTT only when explicitly configured for test.
- [ ] MQTT username/client identity is derived from Device ID according to the accepted policy.
- [ ] MQTT token is derived using versioned HMAC with a product secret, not a plain hash of Device ID.
- [ ] Topic root is `vg/{device_id}/...` with no environment prefix.
- [ ] The device publishes current status using a retained status topic.
- [ ] Requests, responses, telemetry, trend data, events, and alarms are not retained.
- [ ] QoS 0 is used for telemetry, trend, and status.
- [ ] QoS 1 is used for alarms, AI request/response, Candidate Configuration, TTS, voice chunks, OTA, and confirmations.
- [ ] Fixed client ID, clean session, resubscribe-after-reconnect, and LWT are implemented for v1 behavior.
- [ ] MQTT connection state appears in the UI and writes structured events.

## Contract reference

- Authoritative topic/auth/QoS tables: `docs/velaguard-mqtt-contract.md`
- Host-side protocol test without board Ethernet: `tools/ai_bridge_stub/`

## Blocked by

- .scratch/velaguard-independent-edge-ai-gateway/issues/01-bootable-velaguard-skeleton.md
- .scratch/velaguard-independent-edge-ai-gateway/issues/06-rj45-first-network-manager-with-esp01-fallback.md (for on-device MQTT; host stub is unblocked)
