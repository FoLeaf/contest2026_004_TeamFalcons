#!/usr/bin/env python3
"""Validate common consistency rules for generated MThings .mthings XML files."""

from __future__ import annotations

import sys
import xml.etree.ElementTree as ET
import re
from pathlib import Path


def parse_project(path: Path) -> tuple[ET.ElementTree | None, str | None, str | None]:
    raw = path.read_bytes()
    errors: list[str] = []
    for encoding in ("utf-8-sig", "gb18030", "gbk"):
        try:
            text = raw.decode(encoding)
        except UnicodeDecodeError as exc:
            errors.append(f"{encoding}: decode failed at byte {exc.start}")
            continue
        try:
            return ET.ElementTree(ET.fromstring(text)), encoding, None
        except ET.ParseError as exc:
            errors.append(f"{encoding}: XML parse failed: {exc}")
    return None, None, "; ".join(errors)


def parse_extro(text: str | None) -> dict[str, str]:
    values: dict[str, str] = {}
    if not text:
        return values
    for part in text.split(";"):
        if not part or "::" not in part:
            continue
        key, value = part.split("::", 1)
        values[key] = value
    return values


def as_int(value: str | None, default: int = 0) -> int:
    try:
        return int(value) if value not in (None, "") else default
    except ValueError:
        return default


def main() -> int:
    if len(sys.argv) != 2:
        print("Usage: validate_mthings.py <file.mthings>", file=sys.stderr)
        return 2

    path = Path(sys.argv[1])
    errors: list[str] = []
    warnings: list[str] = []

    tree, encoding, parse_error = parse_project(path)
    if tree is None:
        print(f"ERROR: XML parse failed: {parse_error}", file=sys.stderr)
        return 1

    root = tree.getroot()
    if root.tag != "MThings":
        errors.append(f"root tag is {root.tag!r}, expected 'MThings'")

    sys_data = root.find("SYS_DATA")
    device_list = root.find("DEVICE_LIST")
    if sys_data is None:
        errors.append("missing SYS_DATA")
    if device_list is None:
        errors.append("missing DEVICE_LIST")

    if sys_data is None:
        return report(errors, warnings)

    port_names = {"HOST"}
    port_list = root.find("PORT_LIST")
    if port_list is not None:
        for port in list(port_list):
            name = port.get("Name")
            if name:
                port_names.add(name)
            if port.tag == "NET" and name and not re.fullmatch(r"NET\d{3}", name):
                warnings.append(f"NET port {name} does not follow NET### naming")
            if port.tag == "COM" and name and "NET" in name:
                warnings.append(f"COM port {name} contains NET prefix and may be classified as network")

    pages = sys_data.findall("PAGE")
    chip_defs = sys_data.findall("CHIPV")
    page_ids = {page.get("ID") for page in pages}
    chip_ids = [chip.get("ID") for chip in chip_defs]
    chip_id_set = set(chip_ids)

    if not pages:
        errors.append("SYS_DATA has no PAGE")
    if len(chip_ids) != len(chip_id_set):
        errors.append("duplicate top-level SYS_DATA/CHIPV IDs")

    for page in pages:
        page_id = page.get("ID", "")
        width = as_int(page.get("width"))
        height = as_int(page.get("hight"))
        for placement in page.findall("CHIPV"):
            chip_id = placement.get("ID")
            if chip_id not in chip_id_set:
                errors.append(f"PAGE {page_id} references missing CHIPV ID {chip_id}")
            x = as_int(placement.get("PosX"))
            y = as_int(placement.get("PosY"))
            w = as_int(placement.get("Width"))
            h = as_int(placement.get("Height"))
            if x < 0 or y < 0 or w <= 0 or h <= 0:
                errors.append(f"PAGE {page_id} CHIPV {chip_id} has invalid geometry")
            if width > 0 and height > 0 and (x + w > width or y + h > height):
                errors.append(f"PAGE {page_id} CHIPV {chip_id} exceeds page bounds")

    data_keys: set[tuple[str | None, str | None]] = set()
    if device_list is not None:
        for device in device_list.findall("DEVICE"):
            device_id = device.get("ID")
            for port in device.findall("./PORTS/*"):
                port_name = port.get("Name")
                if port_name and port_name not in port_names:
                    errors.append(f"DEVICE {device_id} references missing port {port_name}")
            for data in device.findall("./DATA_LIST/DATA"):
                data_keys.add((device_id, data.get("ID")))

    for chip in chip_defs:
        chip_id = chip.get("ID")
        chip_type = as_int(chip.get("Type"), -1)
        extro = parse_extro(chip.get("ExtroPara"))
        if chip_type in (22, 35):
            target_page = extro.get("pageid")
            if target_page and target_page not in page_ids:
                errors.append(f"navigation CHIPV {chip_id} points to missing pageid {target_page}")
        for data in chip.findall("DeviceData"):
            key = (data.get("DeviceID"), data.get("DataID"))
            if key not in data_keys:
                errors.append(f"CHIPV {chip_id} binds missing DeviceData {key[0]}:{key[1]}")

    for section in ("PORT_LIST", "SIGNT", "MQTT", "CURVE", "HISDATA", "ALARM_LIST"):
        if root.find(section) is None:
            warnings.append(f"missing optional serializer section {section}")

    alarm_list = root.find("ALARM_LIST")
    if alarm_list is not None and len(list(alarm_list)) == 0:
        warnings.append("ALARM_LIST is empty; complete UI-like projects usually include TYPELIST")

    if encoding and encoding != "utf-8-sig":
        warnings.append(f"parsed using {encoding}; preserve or normalize encoding intentionally")

    return report(errors, warnings)


def report(errors: list[str], warnings: list[str]) -> int:
    for warning in warnings:
        print(f"WARNING: {warning}")
    for error in errors:
        print(f"ERROR: {error}")
    if errors:
        print(f"FAILED: {len(errors)} error(s), {len(warnings)} warning(s)")
        return 1
    print(f"OK: 0 error(s), {len(warnings)} warning(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
