#!/usr/bin/env python3
"""Offline checks for the MQTT provisioning blob helper."""

from __future__ import annotations

import importlib.util
import json
import os
import subprocess
import sys
import unittest
import hashlib
import hmac
from pathlib import Path


SCRIPTS = Path(__file__).resolve().parents[1]
PACKER_PATH = SCRIPTS / "vg-provision-pack.py"
MQTT_PACKER_PATH = SCRIPTS / "vg-provision-pack-mqtt.py"


def load_module(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class MqttPackerTests(unittest.TestCase):
    def setUp(self) -> None:
        self.packer = load_module("vg_provision_pack_test", PACKER_PATH)
        self.mqtt_packer = load_module("vg_provision_pack_mqtt_test", MQTT_PACKER_PATH)
        self.uid = bytes.fromhex("00112233445566778899aabb")

    def test_payload_has_only_mqtt_fields(self) -> None:
        plain = self.mqtt_packer.mqtt_json("dedicated-user", "dedicated-password")
        payload = json.loads(plain)
        self.assertEqual(
            payload,
            {
                "kind": "mqtt",
                "username": "dedicated-user",
                "password": "dedicated-password",
            },
        )
        self.assertEqual(
            list(payload), ["kind", "username", "password"]
        )

    def test_missing_values_are_rejected(self) -> None:
        with self.assertRaisesRegex(ValueError, "VG_MQTT_USER"):
            self.mqtt_packer.mqtt_json("", "password")
        with self.assertRaisesRegex(ValueError, "VG_MQTT_PASSWORD"):
            self.mqtt_packer.mqtt_json("user", "")

    def test_output_decrypts_to_expected_record(self) -> None:
        plain = self.mqtt_packer.mqtt_json("dedicated-user", "dedicated-password")
        blob = self.packer.seal(self.uid, plain)
        self.assertEqual(len(blob), self.packer.VGPR_HDR_SIZE + len(plain) + self.packer.VGPR_TAG_SIZE)

        stream_key, mac_key = self.packer.derive_keys(self.uid)
        body, tag = blob[:-self.packer.VGPR_TAG_SIZE], blob[-self.packer.VGPR_TAG_SIZE:]
        self.assertEqual(
            tag,
            hmac.new(mac_key, body, hashlib.sha256).digest(),
        )
        nonce = body[8:24]
        recovered = self.packer.stream_xor(stream_key, nonce, body[self.packer.VGPR_HDR_SIZE:])
        self.assertEqual(recovered, plain)
        self.assertEqual(json.loads(recovered)["kind"], "mqtt")

    def test_environment_inputs_never_appear_in_arguments(self) -> None:
        env = {
            "VG_MQTT_USER": "env-only-user",
            "VG_MQTT_PASSWORD": "env-only-password",
            "PATH": os.environ.get("PATH", ""),
        }
        result = subprocess.run(
            [sys.executable, str(MQTT_PACKER_PATH), self.uid.hex()],
            env=env,
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        self.assertEqual(len(result.stdout), 28 + len(
            b'{"kind":"mqtt","username":"env-only-user","password":"env-only-password"}'
        ) + 32)
        self.assertEqual(result.stderr, b"")

    def test_empty_environment_fails_without_output(self) -> None:
        result = subprocess.run(
            [sys.executable, str(MQTT_PACKER_PATH), self.uid.hex()],
            env={"PATH": os.environ.get("PATH", "")},
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        self.assertEqual(result.returncode, 2)
        self.assertEqual(result.stdout, b"")
        self.assertIn(b"VG_MQTT_USER", result.stderr)


if __name__ == "__main__":
    unittest.main()
