#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Minimal MQTT AI Bridge stub for VelaGuard offline protocol tests."""

from __future__ import annotations

import argparse
import json
import sys
import time


def build_response(req: dict, req_id: str | None = None) -> dict:
    """Build a diagnosis-shaped response matching docs/velaguard-mqtt-contract.md §4.3.

    req_id must match the response topic suffix vg/{device_id}/ai/response/{req_id}.
    When the request omits req_id, the caller supplies a generated id for both.
    """
    rid = req_id if req_id is not None else str(req.get("req_id") or "unknown")
    alarm = req.get("alarm") or {}
    if not isinstance(alarm, dict):
        alarm = {}
    level = alarm.get("level", "WARNING")
    temp = alarm.get("temp_c", 0)
    return {
        "req_id": rid,
        "type": req.get("type", "diagnosis"),
        "ok": True,
        "summary": f"stub: 收到{level}，温度约 {temp}",
        "risk_level": "medium" if level != "OK" else "low",
        "possible_causes": ["负载偏高", "散热异常", "传感器安装松动"],
        "recommended_actions": ["检查负载", "检查散热通道", "复测传感器"],
        "need_shutdown": False,
        "confidence": 0.7,
        "source": "ai_bridge_stub",  # stub-only marker; real bridge may omit
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="VelaGuard AI Bridge MQTT stub")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=1883)
    args = parser.parse_args()

    try:
        import paho.mqtt.client as mqtt
    except ImportError:
        print("需要 paho-mqtt: pip3 install paho-mqtt", file=sys.stderr)
        return 1

    def on_connect(client, userdata, flags, reason_code, properties=None):
        # paho v1/v2 compatibility: reason_code may be int or ReasonCode
        rc = int(getattr(reason_code, "value", reason_code))
        print(f"[bridge_stub] connected rc={rc} to {args.host}:{args.port}")
        if rc != 0:
            print(f"[bridge_stub] connect failed, not subscribing (rc={rc})")
            return
        client.subscribe("vg/+/ai/request", qos=1)

    def on_message(client, userdata, msg):
        try:
            payload = msg.payload.decode("utf-8")
            req = json.loads(payload)
        except Exception as exc:  # noqa: BLE001 - stub should stay up
            print(f"[bridge_stub] bad payload on {msg.topic}: {exc}")
            return

        if not isinstance(req, dict):
            print(f"[bridge_stub] payload is not a JSON object on {msg.topic}")
            return

        device_id = req.get("device_id")
        if not device_id:
            parts = msg.topic.split("/")
            # topic shape: vg/{device_id}/ai/request
            device_id = parts[1] if len(parts) >= 4 and parts[0] == "vg" else "unknown"

        req_id = str(req.get("req_id") or f"auto-{int(time.time())}")
        resp = build_response(req, req_id=req_id)
        out_topic = f"vg/{device_id}/ai/response/{req_id}"
        body = json.dumps(resp, ensure_ascii=False)
        client.publish(out_topic, body, qos=1, retain=False)
        print(f"[bridge_stub] {msg.topic} -> {out_topic}")
        print(body)

    # Prefer Callback API v2 when available
    try:
        client = mqtt.Client(
            mqtt.CallbackAPIVersion.VERSION2,
            client_id="velaguard-ai-bridge-stub",
        )
    except Exception:
        client = mqtt.Client(client_id="velaguard-ai-bridge-stub")

    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(args.host, args.port, keepalive=60)
    print("[bridge_stub] waiting for vg/+/ai/request ... Ctrl+C to stop")
    client.loop_forever()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
