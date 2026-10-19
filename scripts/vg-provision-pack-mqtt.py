#!/usr/bin/env python3
"""Build an encrypted VelaGuard MQTT credential blob for NSH delivery.

The username and password are read from environment variables so they never
appear in a command line, temporary plaintext file, or captured log.  The
resulting binary is the same sealed format as vg-provision-pack.py.
"""

from __future__ import annotations

import importlib.util
import json
import os
import sys
from pathlib import Path


PACKER_PATH = Path(__file__).with_name("vg-provision-pack.py")


def load_packer():
    spec = importlib.util.spec_from_file_location("vg_provision_pack", PACKER_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load {PACKER_PATH}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def mqtt_json(username: str, password: str) -> bytes:
    if not username:
        raise ValueError("VG_MQTT_USER is empty")
    if not password:
        raise ValueError("VG_MQTT_PASSWORD is empty")
    return json.dumps(
        {
            "kind": "mqtt",
            "username": username,
            "password": password,
        },
        separators=(",", ":"),
    ).encode()


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <uid-hex-24>", file=sys.stderr)
        return 2

    uid = bytes.fromhex(sys.argv[1])
    if len(uid) != 12:
        print("uid must be 12 bytes (24 hex chars)", file=sys.stderr)
        return 2

    try:
        plain = mqtt_json(
            os.environ.get("VG_MQTT_USER", ""),
            os.environ.get("VG_MQTT_PASSWORD", ""),
        )
        blob = load_packer().seal(uid, plain)
    except ValueError as exc:
        print(str(exc), file=sys.stderr)
        return 2
    finally:
        if "plain" in locals():
            plain = b""

    sys.stdout.buffer.write(blob)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
