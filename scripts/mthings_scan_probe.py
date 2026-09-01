#!/usr/bin/env python3
"""Mirror vg_disc_scan_probe_holding: FC03 @reg 0/1/2, qty 1 then 2 @9600.

Usage (Windows, MThings **stopped** — COM6 free):
  pip install pyserial
  python scripts/mthings_scan_probe.py COM6

With MThings running, COM6 is locked; use board-side vgdiscover on COM3 instead
(see scripts/stage1_scan_1_32.ps1).
"""

from __future__ import annotations

import struct
import sys
import time

try:
    import serial
except ImportError:
    print("pip install pyserial", file=sys.stderr)
    sys.exit(2)


BAUD = 9600
TRY_START = (0, 2, 1)
TRY_QTY = (1, 2)
INTER_MS = 50
RETRY_INTER_MS = 100
READ_TO_S = 2.0


def crc16_modbus(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc & 0xFFFF


def read_holding(ser: serial.Serial, addr: int, start: int, qty: int) -> bytes | None:
    pdu = struct.pack(">BHH", addr, 0x03, start) + struct.pack(">H", qty)
    frame = pdu + struct.pack("<H", crc16_modbus(pdu))
    ser.reset_input_buffer()
    ser.write(frame)
    deadline = time.monotonic() + READ_TO_S
    buf = bytearray()
    while time.monotonic() < deadline:
        chunk = ser.read(ser.in_waiting or 1)
        if chunk:
            buf.extend(chunk)
            if len(buf) >= 5 and buf[0] == addr:
                if buf[1] & 0x80:
                    return None
                if buf[1] == 0x03 and len(buf) >= 3 + buf[2] + 2:
                    return bytes(buf[: 3 + buf[2] + 2])
        else:
            time.sleep(0.01)
    return None


def probe_addr(ser: serial.Serial, addr: int) -> int | None:
    for start in TRY_START:
        for qty in TRY_QTY:
            rsp = read_holding(ser, addr, start, qty)
            if rsp is not None:
                return start
            time.sleep(INTER_MS / 1000.0)
    return None


def main() -> int:
    port = sys.argv[1] if len(sys.argv) > 1 else "COM6"
    addr_min = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    addr_max = int(sys.argv[3]) if len(sys.argv) > 3 else 32

    print(f"scan {port} @9600 addr {addr_min}-{addr_max} (FC03 reg0/1/2 qty1/2)")
    try:
        ser = serial.Serial(port, BAUD, timeout=0.05, write_timeout=1)
    except serial.SerialException as e:
        print(f"open {port} failed: {e}", file=sys.stderr)
        print("If MThings is running, close collection or exit MThings first.", file=sys.stderr)
        return 1

    hits: list[tuple[int, int]] = []

    def one_pass(retry_only: bool) -> None:
        nonlocal hits
        for addr in range(addr_min, addr_max + 1):
            if retry_only and any(a == addr for a, _ in hits):
                continue
            print(f"  probe addr={addr:2d} ... ", end="", flush=True)
            reg = probe_addr(ser, addr)
            if reg is not None:
                if not any(a == addr for a, _ in hits):
                    hits.append((addr, reg))
                print(f"OK reg={reg}")
            else:
                print("miss")
            time.sleep((RETRY_INTER_MS if retry_only else INTER_MS) / 1000.0)

    try:
        one_pass(retry_only=False)
        if len(hits) < (addr_max - addr_min + 1):
            print("\n--- retry pass (missed addrs) ---")
            one_pass(retry_only=True)
    finally:
        ser.close()

    addrs = [a for a, _ in hits]
    print(f"\nfound {len(hits)}/{(addr_max - addr_min + 1)}: {addrs}")
    missing = [a for a in range(addr_min, addr_max + 1) if a not in addrs]
    if missing:
        print(f"missing: {missing}")
    return 0 if len(hits) == (addr_max - addr_min + 1) else 1


if __name__ == "__main__":
    sys.exit(main())
