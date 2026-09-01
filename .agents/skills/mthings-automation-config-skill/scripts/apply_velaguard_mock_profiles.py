#!/usr/bin/env python3
"""Apply semantically appropriate MThings mock profiles to velaguard slave points."""

from __future__ import annotations

import sys
import xml.etree.ElementTree as ET
from pathlib import Path

TARGET = Path("/mnt/c/Users/19y/Documents/mthings/velaguard.mthings")

# Stategy: 0=fixed 1=linear 2=parabola 3=sine 4=square 5=preset


def fixed(base: int, *, x_invl: str = "1000") -> dict:
    return dict(
        Stategy="0", TimeBngrWay="0",
        Coef_a="0", Coef_b="0", Coef_c=str(base), Coef_d="0", Coef_e="0",
        ResetStep="0", XInvl=x_invl,
        MaxRadom=str(base), MinRadom=str(base),
        MaxValue=str(base), MinValue=str(base),
        datarry="", BindDeviceID="0", BindDataID="0",
    )


def linear(start: int, step: int, max_v: int, min_v: int = 0, *, x_invl: str = "1000") -> dict:
    return dict(
        Stategy="1", TimeBngrWay="1",
        Coef_a="0", Coef_b=str(step), Coef_c=str(start), Coef_d="0", Coef_e="0",
        ResetStep="0", XInvl=x_invl,
        MaxRadom=str(max_v), MinRadom=str(min_v),
        MaxValue=str(max_v), MinValue=str(min_v),
        datarry="", BindDeviceID="0", BindDataID="0",
    )


def parabola(a: int, b: str, c: int, max_v: int, min_v: int = 0, *, x_invl: str = "1000") -> dict:
    return dict(
        Stategy="2", TimeBngrWay="1",
        Coef_a=str(a), Coef_b=b, Coef_c=str(c), Coef_d="0", Coef_e="0",
        ResetStep="24", XInvl=x_invl,
        MaxRadom=str(max_v), MinRadom=str(min_v),
        MaxValue=str(max_v), MinValue=str(min_v),
        datarry="", BindDeviceID="0", BindDataID="0",
    )


def sine(a: int, b: str, c: int, max_v: int, min_v: int = 0, *, jitter: int = 0, x_invl: str = "1000") -> dict:
    return dict(
        Stategy="3", TimeBngrWay="1",
        Coef_a=str(a), Coef_b=b, Coef_c=str(c), Coef_d="0", Coef_e="0",
        ResetStep="12", XInvl=x_invl,
        MaxRadom=str(jitter if jitter else max_v),
        MinRadom="0" if jitter else str(min_v),
        MaxValue=str(max_v), MinValue=str(min_v),
        datarry="", BindDeviceID="0", BindDataID="0",
    )


def square(low: int, high: int, *, period: int = 6, x_invl: str = "1000") -> dict:
    return dict(
        Stategy="4", TimeBngrWay="1",
        Coef_a=str(period), Coef_b="0", Coef_c=str(low), Coef_d="0", Coef_e="0",
        ResetStep="0", XInvl=x_invl,
        MaxRadom=str(high), MinRadom=str(low),
        MaxValue=str(high), MinValue=str(low),
        datarry="", BindDeviceID="0", BindDataID="0",
    )


def preset(values: list[int], *, x_invl: str = "0") -> dict:
    lo = min(values)
    hi = max(values)
    body = "\n".join(str(v) for v in values)
    return dict(
        Stategy="5", TimeBngrWay="0",
        Coef_a="0", Coef_b="0", Coef_c=str(values[0]), Coef_d="0", Coef_e="0",
        ResetStep="0", XInvl=x_invl,
        MaxRadom=str(hi), MinRadom=str(lo),
        MaxValue=str(hi), MinValue=str(lo),
        datarry=body, BindDeviceID="0", BindDataID="0",
    )


def profile_for(dev_id: int, dev_name: str, data_id: int, point_name: str) -> dict | None:
    n = point_name
    # --- device 1-3 (original Stage0 slaves) ---
    if dev_id == 1:
        if "湿度" in n:
            return sine(3, "0.01", 600, 100, 0, jitter=5)
        if "温度" in n:
            return sine(2, "0.01", 250, 80, -40, jitter=3)
    if dev_id == 2:
        if "水浸" in n:
            return preset([1, 1, 1, 1, 255, 1, 1, 1])
        if "灵敏" in n:
            return fixed(80, x_invl="5000")
    if dev_id == 3:
        bases = {1: 250, 2: 280, 3: 320, 4: 245}
        return sine(2, "0.01", bases.get(data_id, 250), 850, -200, jitter=2)

    # --- analog / environment ---
    if dev_id == 4:
        return sine(1, "0.01", 15, 100, 0, jitter=8)
    if dev_id == 5:
        return sine(1, "1", 650, 5000, 400, jitter=80)
    if dev_id == 6:
        base = 35 if data_id == 1 else 48
        return sine(1, "0.1", base, 500 if data_id == 1 else 600, 5, jitter=12)
    if dev_id == 7:
        return parabola(4, "5", 200, 200000, 0, x_invl="5000")
    if dev_id == 8:
        return sine(1, "0.1", 55, 120, 30, jitter=4)
    if dev_id == 9:
        return sine(0, "0.1", 1013, 1100, 800, jitter=2)
    if dev_id == 10:
        if "风速" in n:
            return sine(1, "0.1", 35, 60, 0, jitter=6)
        return sine(2, "1", 180, 360, 0, jitter=15)

    # --- process ---
    if dev_id == 11:
        return linear(620, 1, 980, 120, x_invl="2000")
    if dev_id == 12:
        if "累计" in n:
            return linear(12580, 2, 999999, 0)
        return sine(1, "0.01", 125, 200, 0, jitter=10)
    if dev_id == 13:
        return sine(1, "0.001", 85, 1600, 0, jitter=8)

    # --- energy / power ---
    if dev_id == 14:
        if "电能" in n:
            return linear(45678, 3, 999999, 0)
        return sine(2, "0.01", 128, 500, 0, jitter=20)
    if dev_id == 15:
        if "SOC" in n or "电池" in n:
            return linear(880, -1, 100, 20, x_invl="3000")
        return sine(1, "0.1", 42, 100, 5, jitter=6)

    # --- security ---
    if dev_id == 16:
        if "门" in n:
            return preset([1, 1, 2, 2, 1, 255, 1, 1, 2, 1])
        return linear(37, 1, 9999, 0, x_invl="5000")
    if dev_id == 17:
        return square(1, 2, period=8, x_invl="2000")

    # --- mechanical ---
    if dev_id == 18:
        if "振" in n:
            return sine(1, "0.01", 12, 50, 0, jitter=3)
        return sine(1, "0.1", 45, 120, 20, jitter=4)

    # --- gas / air quality ---
    if dev_id == 19:
        return fixed(120, x_invl="2000")
    if dev_id == 20:
        return sine(1, "1", 28, 500, 0, jitter=6)
    if dev_id == 21:
        return sine(1, "0.001", 8, 5000, 0, jitter=2)
    if dev_id == 22:
        return sine(1, "1", 220, 2000, 50, jitter=35)

    # --- control I/O ---
    if dev_id == 23:
        return square(0, 255, period=4, x_invl="1500")
    if dev_id == 24:
        return preset([4369, 8738, 21845, 4369, 13107, 8738])
    if dev_id == 25:
        base = 12000 if data_id == 1 else 8500
        return sine(0, "0.001", base, 20000, 4000, jitter=180)

    # --- drives ---
    if dev_id == 26:
        if "频率" in n:
            return sine(1, "0.01", 3500, 5000, 0, jitter=60)
        return sine(1, "0.01", 1850, 10000, 0, jitter=25)
    if dev_id == 27:
        if "状态" in n:
            return preset([1, 2, 2, 2, 2, 3, 1, 1, 2, 2])
        return sine(1, "0.1", 65, 200, 0, jitter=8)

    # --- HVAC ---
    if dev_id == 28:
        if "设定" in n:
            return fixed(260, x_invl="60000")
        return sine(1, "0.1", 245, 400, 100, jitter=3)

    # --- generator / storage ---
    if dev_id == 29:
        if "转速" in n:
            return sine(2, "1", 1500, 1800, 0, jitter=25)
        return linear(720, -1, 100, 5, x_invl="5000")
    if dev_id == 30:
        if "SOC" in n:
            return linear(765, -1, 1000, 100, x_invl="4000")
        return sine(0, "0.01", 5120, 6000, 4000, jitter=12)

    # --- PV / transformer ---
    if dev_id == 31:
        if "日发" in n:
            return linear(2856, 5, 99990, 0, x_invl="2000")
        return parabola(3, "0.5", 80, 10000, 0, x_invl="3000")
    if dev_id == 32:
        base = 55 if data_id == 1 else 68
        return sine(1, "0.1", base, 1200 if data_id == 1 else 1500, 0, jitter=3)

    return None


def parse_project(path: Path) -> ET.ElementTree:
    raw = path.read_bytes()
    for encoding in ("utf-8-sig", "gb18030", "gbk"):
        try:
            return ET.ElementTree(ET.fromstring(raw.decode(encoding)))
        except (UnicodeDecodeError, ET.ParseError):
            continue
    raise SystemExit(f"cannot parse {path}")


def indent(elem: ET.Element, level: int = 0) -> None:
    i = "\n" + level * "\t"
    if len(elem):
        if not elem.text or not elem.text.strip():
            elem.text = i + "\t"
        for child in elem:
            indent(child, level + 1)
        if not child.tail or not child.tail.strip():  # noqa: PLW2901
            child.tail = i
    if level and (not elem.tail or not elem.tail.strip()):
        elem.tail = i


def apply_profiles(path: Path) -> tuple[int, int]:
    tree = parse_project(path)
    root = tree.getroot()
    device_list = root.find("DEVICE_LIST")
    if device_list is None:
        raise SystemExit("missing DEVICE_LIST")

    updated = 0
    skipped = 0
    for device in device_list.findall("DEVICE"):
        dev_id = int(device.get("ID", "0"))
        dev_name = device.get("Name", "")
        for data in device.findall("DATA_LIST/DATA"):
            data_id = int(data.get("ID", "0"))
            point_name = data.get("Name", "")
            profile = profile_for(dev_id, dev_name, data_id, point_name)
            if profile is None:
                skipped += 1
                continue
            value = data.find("VALUE")
            if value is None:
                value = ET.SubElement(data, "VALUE")
            for key, val in profile.items():
                value.set(key, val)
            updated += 1

    indent(root)
    tree.write(path, encoding="UTF-8", xml_declaration=True)
    return updated, skipped


def main() -> int:
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else TARGET
    updated, skipped = apply_profiles(path)
    print(f"Updated {updated} VALUE mock profile(s) in {path} ({skipped} unmatched)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
