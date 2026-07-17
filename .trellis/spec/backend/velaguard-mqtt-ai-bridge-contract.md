# VelaGuard MQTT / AI Bridge Contract

> Host-side protocol freeze for offline development. Board MQTT client is deferred until RJ45.

Canonical human-readable contract: `docs/velaguard-mqtt-contract.md`  
Host stub: `tools/ai_bridge_stub/bridge_stub.py`

---

## Scenario: Board ↔ Broker ↔ AI Bridge (diagnosis)

### 1. Scope / Trigger

- Trigger: any work that publishes/subscribes MQTT topics, AI diagnosis request/response JSON, or host bridge stubs.
- Out of scope for v1 host stub: real MiMo HTTPS, MQTTS, board Ethernet client, OTA binary transfer.

### 2. Signatures

| Role | Interface |
|------|-----------|
| Topic root | `vg/{device_id}/...` |
| Device client id | `vg-{device_id}` (recommended) |
| Stub CLI | `python3 tools/ai_bridge_stub/bridge_stub.py [--host HOST] [--port PORT]` |
| Stub subscribe | `vg/+/ai/request` QoS 1 |
| Stub publish | `vg/{device_id}/ai/response/{req_id}` QoS 1, retain=false |

Auth (documented; production path):

- Username = `device_id`
- Password = `HMAC-SHA256(PRODUCT_AUTH_SECRET, "velaguard:mqtt:v1:" + device_id)`
- Trial LAN may use plaintext MQTT; must not ship as production default.

### 3. Contracts

#### Topics (v1)

| Topic | Direction | QoS | retained |
|-------|-----------|----:|----------|
| `vg/{id}/status` | device→cloud | 0 | yes |
| `vg/{id}/telemetry` | device→cloud | 0 | no |
| `vg/{id}/alarm` | device→cloud | 1 | no |
| `vg/{id}/event` | device→cloud | 1 | no |
| `vg/{id}/ai/request` | device→cloud | 1 | no |
| `vg/{id}/ai/response/{req_id}` | cloud→device | 1 | no |
| `vg/{id}/config/candidate` | cloud→device | 1 | no |
| `vg/{id}/tts/request` | device→cloud | 1 | no |
| `vg/{id}/tts/response/{req_id}` | cloud→device | 1 | no |

#### `ai/request` (diagnosis minimum)

| Field | Type | Required | Notes |
|-------|------|----------|-------|
| `req_id` | string | yes | echoed in response topic suffix and body |
| `type` | string | yes | `"diagnosis"` for this path |
| `device_id` | string | yes | if missing, stub may parse from topic `vg/{id}/ai/request` |
| `alarm` | object | no | level/summary/temp_c/threshold_c |
| `context` | object | no | backend, recent_events |

#### `ai/response/{req_id}` (diagnosis)

| Field | Type | Required | Notes |
|-------|------|----------|-------|
| `req_id` | string | yes | **must equal** topic suffix |
| `type` | string | yes | `"diagnosis"` |
| `ok` | bool | yes | |
| `summary` | string | yes | |
| `risk_level` | string | yes | e.g. low/medium/high |
| `possible_causes` | string[] | yes | |
| `recommended_actions` | string[] | yes | |
| `need_shutdown` | bool | yes | advisory only; local confirm required |
| `confidence` | number | yes | 0..1 |
| `source` | string | no | stub may set `ai_bridge_stub`; parsers ignore unknowns |

### 4. Validation & Error Matrix

| Condition | Behavior |
|-----------|----------|
| Non-JSON / non-object payload on ai/request | stub logs and **does not** publish response |
| Missing `req_id` | stub generates one id and uses it for **both** topic and body |
| Missing `device_id` | stub uses topic segment if topic matches `vg/{id}/ai/request` |
| Connect rc ≠ 0 | stub does not subscribe |
| AI advice vs control | AI output is advisory; config/control writes need local confirm (ADR 0004) |
| Local safety loop | acq/alarm/log must work with MQTT offline |

### 5. Good / Base / Bad Cases

- **Good**: board (or mosquitto_pub) sends full diagnosis request → response on matching topic with same `req_id`.
- **Base**: host-only: local mosquitto + `bridge_stub.py` + mosquitto_sub on `vg/+/ai/response/#`.
- **Bad**: response topic uses one `req_id` while body uses another; board cannot correlate.

### 6. Tests Required

| Test | Assert |
|------|--------|
| `python3 -m py_compile tools/ai_bridge_stub/bridge_stub.py` | syntax OK |
| Unit: `build_response` without broker | core diagnosis fields present; `req_id` consistent |
| Manual smoke (optional if mosquitto+paho installed) | pub request → sub sees response topic+JSON |

### 7. Wrong vs Correct

#### Wrong

```text
publish vg/dev1/ai/response/auto-123  body: {"req_id":"unknown", ...}
```

#### Correct

```text
publish vg/dev1/ai/response/req-001  body: {"req_id":"req-001", "ok":true, ...}
```

---

## Design Decisions

- **Host stub before board Ethernet**: freezes protocol while RJ45 unavailable.
- **Board never calls MiMo HTTPS**: only MQTT to bridge.
- **No environment prefix in topic root** (`vg/{id}/...` only) for v1 simplicity.

## Common Mistakes

- Treating stub `source` as required on production bridge.
- Implementing board MQTT client in the same task as the contract freeze without RJ45.
- Using retained messages on ai/request or ai/response (must not retain).
