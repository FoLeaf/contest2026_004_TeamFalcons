#!/usr/bin/env python3
"""Export velaguard.mthings holding-register layout for Modbus Slave mock."""

from __future__ import annotations

import csv
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
DEFAULT_MTHINGS = REPO / "config/mthings/velaguard.mthings"
OUT_CSV = REPO / "config/modbus-slave/velaguard_slaves.csv"


def export_slaves(mthings: Path) -> list[dict]:
    root = ET.parse(mthings).getroot()
    dl = root.find("DEVICE_LIST")
    if dl is None:
        raise SystemExit(f"No DEVICE_LIST in {mthings}")

    rows: list[dict] = []
    for dev in dl.findall("DEVICE"):
        addr = int(dev.get("Addr", "0"))
        name = dev.get("Name", "")
        first_reg: int | None = None
        max_reg = 0
        seed: dict[int, int] = {}

        data_list = dev.find("DATA_LIST")
        if data_list is None:
            continue

        for d in data_list.findall("DATA"):
            if d.get("BLOCK") != "2":
                continue
            reg = int(d.get("Addr", "0"))
            sz = int(d.get("sz", "2"))
            end = reg + (sz // 2) - 1
            if first_reg is None or reg < first_reg:
                first_reg = reg
            max_reg = max(max_reg, end)
            raw = d.get("Value", "0")
            try:
                seed[reg] = int(raw, 0) if raw.lower().startswith("0x") else int(float(raw))
            except ValueError:
                seed[reg] = 0

        fr = first_reg if first_reg is not None else 0
        qty = max(max_reg + 1, 1)
        rows.append(
            {
                "addr": addr,
                "name": name,
                "first_reg": fr,
                "holding_qty": qty,
                "seed_reg": fr,
                "seed_value": seed.get(fr, 100 if addr == 1 else 1),
            }
        )

    rows.sort(key=lambda r: r["addr"])
    return rows


def main() -> int:
    src = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_MTHINGS
    if not src.is_file():
        print(f"Missing {src}", file=sys.stderr)
        return 1

    rows = export_slaves(src)
    OUT_CSV.parent.mkdir(parents=True, exist_ok=True)
    fields = ["addr", "name", "first_reg", "holding_qty", "seed_reg", "seed_value"]
    with OUT_CSV.open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        w.writerows(rows)

    print(f"Wrote {len(rows)} slaves -> {OUT_CSV}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
