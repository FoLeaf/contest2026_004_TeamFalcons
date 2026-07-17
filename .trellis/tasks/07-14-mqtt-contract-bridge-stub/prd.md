# MQTT topic contract and host AI Bridge stub without board Ethernet

## Goal

Document MQTT topics/auth and provide a host-side AI Bridge stub for offline development; board remains offline until RJ45 available.

## Requirements

1. Publish a frozen MQTT topic/auth/message contract under `docs/` for board and cloud to share.
2. Provide a host-side AI Bridge stub that can run without board Ethernet (PC + local broker only).
3. Stub must subscribe to `vg/+/ai/request` and publish a diagnosis-shaped response on `vg/{device_id}/ai/response/{req_id}`.
4. Do **not** change board firmware network path in this task; board stays offline until RJ45 work.
5. Document how to self-test with mosquitto_pub/sub.

## Constraints

- No real MiMo HTTPS calls in the stub.
- Trial plaintext MQTT on LAN is OK; production path remains MQTTS + per-device token (documented, not implemented here).
- Local safety loop (acq/alarm) remains independent of MQTT.

## Acceptance Criteria

- [x] `docs/velaguard-mqtt-contract.md` exists with topic tree, auth rules, status/ai request/response JSON skeletons.
- [x] `tools/ai_bridge_stub/bridge_stub.py` + README exist and parse cleanly.
- [x] Stub maps request topic → response topic with fixed diagnosis fields (`ok`, `summary`, `risk_level`, etc.).
- [x] README documents broker + stub + mosquitto_pub/sub smoke steps.
- [x] No board Ethernet/MQTT client code required for this task.

## Notes

- Lightweight host-side contract task; board client is a follow-up after RJ45.
