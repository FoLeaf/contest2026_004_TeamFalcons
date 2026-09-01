#!/usr/bin/env python3
"""Generate a complete MThings .mthings project from protocol point tables.

Input formats: CSV, TSV, or JSON array of row objects.

The command name is kept for backward compatibility with earlier Modbus-only
workflows. Rows can now describe Modbus, Siemens S7, DL/T645, CJ/T188,
DL/T698.45, or local tags.
"""

from __future__ import annotations

import argparse
import csv
import json
import re
import xml.etree.ElementTree as ET
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


COL_ALIASES = {
    "name": ("name", "tag", "point", "signal", "description", "desc", "variable"),
    "address": ("address", "addr", "register", "reg", "offset"),
    "block": ("block", "area", "register_type", "reg_type", "function", "func", "fc"),
    "data_type": ("data_type", "type", "datatype", "value_type"),
    "unit": ("unit", "units"),
    "gain": ("gain", "scale", "factor"),
    "offset": ("offset",),
    "quantity": ("quantity", "size", "qty", "registers", "length"),
    "decimals": ("decimals", "point", "precision"),
    "min": ("min", "minimum", "low"),
    "max": ("max", "maximum", "high"),
    "range": ("range",),
    "access": ("access", "rw", "read_write"),
    "group": ("group", "category"),
    "enum": ("enum", "enums", "mapping", "value_map"),
    "alarm_hi": ("alarm_hi", "hi_alarm", "high_alarm"),
    "alarm_lo": ("alarm_lo", "lo_alarm", "low_alarm"),
    "trend": ("trend", "curve"),
    "history": ("history", "his"),
    "byte_order": ("byte_order", "byteoder", "byte_order_mode"),
    "word_order": ("word_order", "wordoder", "word_order_mode"),
    "interval_time": ("interval_time", "poll_interval", "interval"),
    "value_strategy": ("value_strategy", "strategy", "stategy"),
    "protocol": ("protocol", "protocol_family", "family", "prtc", "comm_protocol"),
    "device": ("device", "device_name", "station", "meter", "plc"),
    "device_id": ("device_id", "devid", "dev_id"),
    "device_type": ("device_type", "dev_type", "role"),
    "port": ("port", "port_name", "channel", "link"),
    "station": ("station", "slave_addr", "slave", "unit_id"),
    "ip": ("ip", "host", "des_ip", "remote_ip", "address_ip"),
    "tcp_port": ("tcp_port", "des_port", "remote_port", "port_id"),
    "serial_baud": ("baud", "baund", "serial_baud"),
    "serial_parity": ("parity",),
    "serial_data_bit": ("data_bit", "databit", "data_bits"),
    "serial_stop_bit": ("stop_bit", "stopbit", "stop_bits"),
    "s7_area": ("s7_area", "area"),
    "s7_db_no": ("s7_db_no", "db_no", "db", "dbno"),
    "s7_byte_offset": ("s7_byte_offset", "byte_offset", "byte"),
    "s7_bit_offset": ("s7_bit_offset", "bit_offset", "bit"),
    "s7_cpu_model": ("s7_cpu_model", "cpu_model"),
    "s7_rack": ("s7_rack", "rack"),
    "s7_slot": ("s7_slot", "slot"),
    "s7_pack_way": ("s7_pack_way", "pack_way_s7"),
    "dlt645_addr": ("dlt645_addr", "meter_addr", "meter_address"),
    "dlt645_di": ("dlt645_di", "di", "data_identifier"),
    "dlt645_password": ("dlt645_password", "password"),
    "dlt645_operator": ("dlt645_operator", "operator"),
    "dlt645_send_wake": ("dlt645_send_wake", "send_wake", "wake_head"),
    "cjt188_addr": ("cjt188_addr", "cjt188_meter_addr"),
    "cjt188_meter_type": ("cjt188_meter_type", "meter_type"),
    "cjt188_di": ("cjt188_di", "di", "data_identifier"),
    "cjt188_data_offset": ("cjt188_data_offset", "cjt188_offset", "data_offset"),
    "cjt188_send_wake": ("cjt188_send_wake", "send_wake", "wake_head", "preamble_bytes"),
    "cjt188_allow_reversed_di": ("cjt188_allow_reversed_di", "allow_reversed_di"),
    "dlt69845_sa": ("dlt69845_sa", "dlt69845_addr", "dlt698_sa", "sa", "server_address"),
    "dlt69845_oi": ("dlt69845_oi", "dlt698_oi", "oi", "object_identifier"),
    "dlt69845_attr": ("dlt69845_attr", "dlt698_attr", "attr", "attribute", "attribute_no"),
    "dlt69845_index": ("dlt69845_index", "dlt698_index", "attr_index", "attribute_index"),
    "dlt69845_ca": ("dlt69845_ca", "dlt698_ca", "ca", "client_address"),
    "dlt69845_send_wake": ("dlt69845_send_wake", "send_wake", "wake_head", "preamble_bytes"),
    "device_poll_interval": ("device_poll_interval", "device_polling_interval", "p_invl"),
}


TRANS_MODE_MBRTU = 0
TRANS_MODE_MBASCII = 1
TRANS_MODE_MBTCP_SYN = 2
TRANS_MODE_MBTCP_ASYN = 3
TRANS_MODE_S7 = 4
TRANS_MODE_DLT645 = 5
TRANS_MODE_CJT188 = 6
TRANS_MODE_DLT69845 = 7

TCP_LINK_CLIENT = 0
TCP_LINK_SERVER = 1
UDP_LINK_UNICAST = 2

DT_DEV_MBMST = 1
DT_DEV_MBSLV = 2
DT_DEV_LOCAL = 4

STT_INT = 0
STT_UINT = 1
STT_FLOAT = 2
STT_BYTES = 3
STT_BIT = 4
STT_BCD = 5
STT_BCD_S = 6
STT_SELF_DESC = 7

SST_FLOAT = 0
SST_INT_BIT = 1
SST_INT_DEC = 2
SST_INT_HEX = 3
SST_BYTES = 4
SST_STRING = 5
SST_TIME = 6
SST_ENUM = 7

S7_AREAS = {"db": 0, "m": 1, "i": 2, "q": 3, "ct": 4, "c": 4, "tm": 5, "t": 5}


@dataclass
class Point:
    device_key: str
    data_id: int
    name: str
    protocol: str
    block: int
    addr: int
    s7_area: int | None
    s7_db_no: int
    s7_byte_offset: int
    dlt645_di: int | None
    cjt188_di: int | None
    cjt188_data_offset: int
    dlt69845_oi: int | None
    dlt69845_attr: int
    dlt69845_index: int
    prtc_type: int
    show_type: int
    byte_size: int
    bit_num: int
    bit_offset: int
    point_num: int
    unit: str
    gain: str
    offset: str
    range_text: str
    min_value: str
    max_value: str
    color: str
    group: str
    access: str
    enum_items: list[tuple[str, str]]
    trend: bool
    history: bool
    byte_order: str
    word_order: str
    interval_time: str
    value_strategy: str


@dataclass
class DeviceSpec:
    key: str
    device_id: int
    name: str
    protocol: str
    device_type: int
    port: str
    modbus_addr: int
    dlt645_addr: str
    dlt645_password: str
    dlt645_operator: str
    cjt188_addr: str
    dlt69845_sa: str
    ip: str
    tcp_port: int
    serial_baud: int
    serial_parity: int
    serial_data_bit: int
    serial_stop_bit: int
    s7_cpu_model: int
    s7_rack: int
    s7_slot: int
    s7_pack_way: int
    dlt645_send_wake: int
    cjt188_send_wake: int
    cjt188_allow_reversed_di: int
    dlt69845_ca: int
    dlt69845_send_wake: int
    poll_interval: int


def norm_key(key: str) -> str:
    return re.sub(r"[^a-z0-9]+", "_", key.strip().lower()).strip("_")


def normalize_row(row: dict[str, object]) -> dict[str, str]:
    source = {norm_key(str(k)): "" if v is None else str(v).strip() for k, v in row.items()}
    normalized: dict[str, str] = {}
    for target, aliases in COL_ALIASES.items():
        for alias in aliases:
            key = norm_key(alias)
            if key in source and source[key] != "":
                normalized[target] = source[key]
                break
    return normalized


def read_rows(path: Path) -> list[dict[str, str]]:
    if path.suffix.lower() == ".json":
        data = json.loads(path.read_text(encoding="utf-8-sig"))
        if not isinstance(data, list):
            raise ValueError("JSON input must be an array of row objects")
        return [normalize_row(row) for row in data if isinstance(row, dict)]

    sample = path.read_text(encoding="utf-8-sig", errors="replace")[:4096]
    delimiter = "\t" if path.suffix.lower() == ".tsv" else ","
    if path.suffix.lower() not in (".csv", ".tsv"):
        try:
            delimiter = csv.Sniffer().sniff(sample).delimiter
        except csv.Error:
            pass
    with path.open("r", encoding="utf-8-sig", newline="") as handle:
        return [normalize_row(row) for row in csv.DictReader(handle, delimiter=delimiter)]


def truthy(value: str) -> bool:
    return value.strip().lower() in ("1", "true", "yes", "y", "on", "curve", "trend", "history")


def parse_int(value: str, default: int = 0) -> int:
    if value is None or str(value).strip() == "":
        return default
    text = str(value).strip()
    try:
        return int(text, 0)
    except ValueError:
        match = re.search(r"\d+", text)
        return int(match.group(0)) if match else default


def parse_hex_or_int(value: str, default: int = 0) -> int:
    text = str(value or "").strip()
    if not text:
        return default
    try:
        return int(text, 0)
    except ValueError:
        return int(re.sub(r"[^0-9A-Fa-f]", "", text) or str(default), 16)


def protocol_of(row: dict[str, str]) -> str:
    explicit = row.get("protocol", "").strip().lower()
    if explicit:
        if explicit in ("s7", "siemens", "siemens_s7", "snap7"):
            return "s7"
        if explicit in ("dlt645", "dl/t645", "dl645", "645"):
            return "dlt645"
        if explicit in ("cjt188", "cj/t188", "cj188", "188"):
            return "cjt188"
        if explicit in ("dlt69845", "dl/t69845", "dl/t698.45", "dlt698", "dl/t698", "698", "698.45"):
            return "dlt69845"
        if explicit in ("host", "local"):
            return "host"
        if explicit in ("modbus", "mb", "modbus_tcp", "modbus_rtu", "rtu", "tcp"):
            return "modbus"

    text = " ".join(
        row.get(key, "")
        for key in ("protocol", "port", "block", "s7_area", "dlt645_di", "cjt188_di", "dlt69845_oi")
    ).lower()
    if "698" in text or row.get("dlt69845_oi") or row.get("dlt69845_sa"):
        return "dlt69845"
    if "188" in text or row.get("cjt188_di") or row.get("cjt188_addr"):
        return "cjt188"
    if "645" in text or row.get("dlt645_di") or row.get("dlt645_addr"):
        return "dlt645"
    if "s7" in text or row.get("s7_area") or row.get("s7_db_no") or row.get("s7_byte_offset"):
        return "s7"
    if "host" in text or "local" in text:
        return "host"
    return "modbus"


def infer_block_and_addr(row: dict[str, str]) -> tuple[int, int]:
    block_text = row.get("block", "").strip().lower()
    addr_text = row.get("address", "0").strip()
    raw_addr = parse_int(addr_text, 0)

    block_map = {
        "coil": 0,
        "coils": 0,
        "0x": 0,
        "01": 0,
        "fc1": 0,
        "discrete": 1,
        "discrete input": 1,
        "input bit": 1,
        "1x": 1,
        "02": 1,
        "fc2": 1,
        "holding": 2,
        "holding register": 2,
        "4x": 2,
        "03": 2,
        "fc3": 2,
        "input": 3,
        "input register": 3,
        "3x": 3,
        "04": 3,
        "fc4": 3,
    }
    block = None
    for key, value in block_map.items():
        if key in block_text:
            block = value
            break
    if block is None and block_text.isdigit():
        block = {"1": 0, "2": 1, "3": 2, "4": 3}.get(block_text, parse_int(block_text, 2))

    if raw_addr >= 40001:
        return 2 if block is None else block, raw_addr - 40001
    if 30001 <= raw_addr < 40000:
        return 3 if block is None else block, raw_addr - 30001
    if 10001 <= raw_addr < 20000:
        return 1 if block is None else block, raw_addr - 10001
    if block is None:
        block = 2
    if block in (0, 1) and raw_addr >= 1 and addr_text.startswith("0"):
        return block, raw_addr - 1
    return block, raw_addr


def parse_enum(text: str) -> list[tuple[str, str]]:
    items: list[tuple[str, str]] = []
    if not text:
        return items
    for part in re.split(r"[;|,]+", text):
        if not part.strip():
            continue
        if ":" in part:
            value, name = part.split(":", 1)
        elif "=" in part:
            value, name = part.split("=", 1)
        else:
            continue
        items.append((value.strip(), name.strip()))
    return items


def infer_type(row: dict[str, str], block: int) -> tuple[int, int, int, int]:
    data_type = row.get("data_type", "").lower()
    quantity = max(0, parse_int(row.get("quantity", ""), 0))
    enum_items = parse_enum(row.get("enum", ""))

    if "self_desc" in data_type or "self-described" in data_type or "axdr" in data_type:
        return STT_SELF_DESC, SST_BYTES, quantity or 1, max(8, quantity * 8)
    if block in (0, 1) or any(token in data_type for token in ("bool", "coil", "bit")):
        return STT_BIT, SST_INT_BIT if not enum_items else SST_ENUM, quantity or 1, 1
    if "float" in data_type or "real" in data_type:
        return STT_FLOAT, SST_FLOAT, quantity or 2, 32
    if "double" in data_type or "float64" in data_type:
        return STT_FLOAT, SST_FLOAT, quantity or 4, 64
    if "string" in data_type or "char" in data_type:
        return STT_BYTES, SST_STRING, max(1, quantity), max(8, quantity * 8)
    if "bcd_s" in data_type or "signed bcd" in data_type:
        return STT_BCD_S, SST_FLOAT, quantity or 2, 32
    if "bcd" in data_type:
        return STT_BCD, SST_FLOAT, quantity or 2, 32
    if "uint32" in data_type or "dword" in data_type:
        return STT_UINT, SST_INT_DEC, quantity or 2, 32
    if "int32" in data_type:
        return STT_INT, SST_INT_DEC, quantity or 2, 32
    if "int" in data_type and "uint" not in data_type:
        return STT_INT, SST_INT_DEC if not enum_items else SST_ENUM, quantity or 1, 16
    return STT_UINT, SST_INT_DEC if not enum_items else SST_ENUM, quantity or 1, 16


def device_type_of(row: dict[str, str], protocol: str) -> int:
    text = row.get("device_type", "").strip().lower()
    if text in ("4", "host", "local"):
        return DT_DEV_LOCAL
    if text in ("2", "slave", "server", "modbus_slave"):
        return DT_DEV_MBSLV
    if protocol == "host":
        return DT_DEV_LOCAL
    return DT_DEV_MBMST


def normalize_dlt645_addr(text: str, fallback: int) -> str:
    digits = re.sub(r"\D", "", text or "")
    if 1 <= len(digits) <= 12:
        return digits.rjust(12, "0")
    return f"{fallback:012d}"


def parse_cjt188_meter_type(text: str, default: int = 0x10) -> int:
    value = str(text or "").strip().lower()
    if value.endswith("h"):
        value = value[:-1]
    try:
        meter_type = int(value, 16) if value else default
    except ValueError:
        return default
    valid = 0x10 <= meter_type <= 0x13 or 0x20 <= meter_type <= 0x22 or meter_type == 0x30 or 0x40 <= meter_type <= 0x49
    return meter_type if valid else default


def normalize_cjt188_addr(text: str, meter_type_text: str, fallback: int) -> str:
    compact = re.sub(r"\s+", "", text or "")
    if re.fullmatch(r"[0-9A-Fa-f]{2}[0-9]{14}", compact):
        meter_type = parse_cjt188_meter_type(compact[:2], -1)
        if meter_type >= 0 and compact[2:] != "9" * 14:
            return f"{meter_type:02X}{compact[2:]}"

    digits = re.sub(r"\D", "", text or "")
    meter_addr = digits[-14:].rjust(14, "0") if digits else f"{fallback:014d}"
    if meter_addr == "9" * 14:
        meter_addr = f"{fallback:014d}"
    return f"{parse_cjt188_meter_type(meter_type_text):02X}{meter_addr}"


def normalize_dlt69845_sa(text: str, fallback: int) -> str:
    digits = re.sub(r"\D", "", text or "")
    if 1 <= len(digits) <= 32 and any(char != "0" for char in digits):
        return digits
    return f"{fallback:012d}"


def default_port(row: dict[str, str], protocol: str, device_type: int, index: int, pack_demo_style: bool) -> str:
    explicit = row.get("port", "").strip()
    if explicit:
        return explicit
    if device_type == DT_DEV_LOCAL or protocol == "host":
        return "HOST"
    if protocol in ("modbus", "dlt645", "cjt188", "dlt69845") and not row.get("ip") and row.get("serial_baud"):
        return f"COM{index}"
    net_index = index - 1 if pack_demo_style else index
    return f"NET{net_index:03d}"


def build_devices(rows: list[dict[str, str]], default_name: str, default_device_type: int, pack_demo_style: bool) -> list[DeviceSpec]:
    devices: list[DeviceSpec] = []
    by_key: dict[str, DeviceSpec] = {}
    for row in rows:
        protocol = protocol_of(row)
        device_type = device_type_of(row, protocol)
        if protocol != "host" and default_device_type in (DT_DEV_MBMST, DT_DEV_MBSLV, DT_DEV_LOCAL) and not row.get("device_type"):
            device_type = default_device_type
        key = row.get("device_id") or row.get("device") or row.get("port") or f"{protocol}:1"
        if key in by_key:
            continue
        device_id = parse_int(row.get("device_id", ""), len(devices) + 1)
        name = row.get("device") or (default_name if len(devices) == 0 else f"{default_name} {len(devices) + 1}")
        port = default_port(row, protocol, device_type, len(devices) + 1, pack_demo_style)
        modbus_addr = parse_int(row.get("address", ""), len(devices) + 1)
        if row.get("station"):
            modbus_addr = parse_int(row.get("station", ""), modbus_addr)
        spec = DeviceSpec(
            key=key,
            device_id=device_id,
            name=name,
            protocol=protocol,
            device_type=device_type,
            port=port,
            modbus_addr=max(0, min(255, modbus_addr)),
            dlt645_addr=normalize_dlt645_addr(row.get("dlt645_addr", ""), len(devices) + 1),
            dlt645_password=row.get("dlt645_password", ""),
            dlt645_operator=row.get("dlt645_operator", ""),
            cjt188_addr=normalize_cjt188_addr(
                row.get("cjt188_addr", ""), row.get("cjt188_meter_type", ""), len(devices) + 1
            ),
            dlt69845_sa=normalize_dlt69845_sa(row.get("dlt69845_sa", ""), len(devices) + 1),
            ip=row.get("ip", "127.0.0.1") or "127.0.0.1",
            tcp_port=parse_int(
                row.get("tcp_port", ""),
                102 if protocol == "s7" else 20000 if protocol == "dlt645" else 20001 if protocol == "cjt188" else 698 if protocol == "dlt69845" else 502,
            ),
            serial_baud=parse_int(row.get("serial_baud", ""), 2400 if protocol in ("dlt645", "cjt188", "dlt69845") else 9600),
            serial_parity=parse_int(row.get("serial_parity", ""), 2 if protocol in ("dlt645", "cjt188", "dlt69845") else 0),
            serial_data_bit=parse_int(row.get("serial_data_bit", ""), 0),
            serial_stop_bit=parse_int(row.get("serial_stop_bit", ""), 0),
            s7_cpu_model=parse_int(row.get("s7_cpu_model", ""), 0),
            s7_rack=parse_int(row.get("s7_rack", ""), 0),
            s7_slot=parse_int(row.get("s7_slot", ""), 2),
            s7_pack_way=parse_int(row.get("s7_pack_way", ""), 1),
            dlt645_send_wake=parse_int(row.get("dlt645_send_wake", ""), 1),
            cjt188_send_wake=max(0, min(4, parse_int(row.get("cjt188_send_wake", ""), 2))),
            cjt188_allow_reversed_di=max(0, min(1, parse_int(row.get("cjt188_allow_reversed_di", ""), 0))),
            dlt69845_ca=max(0, min(255, parse_int(row.get("dlt69845_ca", ""), 0))),
            dlt69845_send_wake=max(0, min(4, parse_int(row.get("dlt69845_send_wake", ""), 4))),
            poll_interval=max(0, parse_int(row.get("device_poll_interval", ""), 1)),
        )
        by_key[key] = spec
        devices.append(spec)
    return devices


def build_points(rows: Iterable[dict[str, str]], devices: list[DeviceSpec]) -> list[Point]:
    colors = ["#FFE74C3C", "#FF3498DB", "#FF2ECC71", "#FFF59E0B", "#FF6366F1", "#FF10B981"]
    points: list[Point] = []
    counters = {device.key: 0 for device in devices}
    device_by_key = {device.key: device for device in devices}
    for index, row in enumerate(rows, start=1):
        protocol = protocol_of(row)
        device_key = row.get("device_id") or row.get("device") or row.get("port") or f"{protocol}:1"
        if device_key not in device_by_key:
            device_key = devices[0].key
        counters[device_key] += 1
        name = row.get("name") or f"Register {index}"
        block, addr = infer_block_and_addr(row)
        prtc_type, show_type, quantity, bit_num = infer_type(row, block)
        if protocol == "dlt69845" and not row.get("data_type"):
            prtc_type, show_type, quantity, bit_num = STT_SELF_DESC, SST_BYTES, 1, 8
        enum_items = parse_enum(row.get("enum", ""))
        min_value = row.get("min", "")
        max_value = row.get("max", "")
        range_text = row.get("range") or (f"{min_value}-{max_value}" if min_value or max_value else "")
        point_num = parse_int(row.get("decimals", ""), 0 if show_type != 0 else 1)
        trendable = show_type == 0 or (prtc_type in (0, 1, 2) and show_type in (0, 2))
        s7_area_text = row.get("s7_area") or row.get("block", "")
        s7_area = S7_AREAS.get(s7_area_text.strip().lower(), 0) if protocol == "s7" else None
        s7_byte_offset = parse_int(row.get("s7_byte_offset", "") or row.get("address", ""), 0)
        dlt645_di = parse_hex_or_int(row.get("dlt645_di", "") or row.get("address", ""), 0) if protocol == "dlt645" else None
        cjt188_di = min(0xFFFF, parse_hex_or_int(row.get("cjt188_di", "") or row.get("address", ""), 0)) if protocol == "cjt188" else None
        dlt69845_oi = min(0xFFFF, parse_hex_or_int(row.get("dlt69845_oi", "") or row.get("address", ""), 0)) if protocol == "dlt69845" else None
        if protocol in ("dlt645", "cjt188", "dlt69845"):
            if row.get("quantity"):
                byte_size = max(1, parse_int(row.get("quantity", ""), 1))
            elif prtc_type == STT_FLOAT:
                byte_size = 4
            elif prtc_type in (STT_BCD, STT_BCD_S):
                byte_size = 4
            elif prtc_type in (STT_INT, STT_UINT):
                byte_size = 2
            else:
                byte_size = 1
            bit_num = 1 if prtc_type == STT_BIT else byte_size * 8
        else:
            byte_size = 1 if prtc_type == STT_BIT and protocol == "s7" else max(1, quantity * 2)
        point = Point(
            device_key=device_key,
            data_id=counters[device_key],
            name=name,
            protocol=protocol,
            block=block,
            addr=addr,
            s7_area=s7_area,
            s7_db_no=parse_int(row.get("s7_db_no", ""), 1 if s7_area == 0 else 0),
            s7_byte_offset=s7_byte_offset,
            dlt645_di=dlt645_di,
            cjt188_di=cjt188_di,
            cjt188_data_offset=max(0, min(max(0, 252 - byte_size), parse_int(row.get("cjt188_data_offset", ""), 0))),
            dlt69845_oi=dlt69845_oi,
            dlt69845_attr=max(0, min(0x1F, parse_int(row.get("dlt69845_attr", ""), 2))),
            dlt69845_index=max(0, min(0xFF, parse_int(row.get("dlt69845_index", ""), 0))),
            prtc_type=prtc_type,
            show_type=show_type,
            byte_size=byte_size,
            bit_num=bit_num,
            bit_offset=parse_int(row.get("s7_bit_offset", ""), parse_int(row.get("bit_offset", ""), 0)),
            point_num=point_num,
            unit=row.get("unit", ""),
            gain=row.get("gain", "1") or "1",
            offset=row.get("offset", "0") or "0",
            range_text=range_text,
            min_value=min_value or "0",
            max_value=max_value or "100",
            color=colors[(index - 1) % len(colors)],
            group=row.get("group", "Process") or "Process",
            access=row.get("access", "R").upper(),
            enum_items=enum_items,
            trend=truthy(row.get("trend", "")) or (trendable and index <= 3),
            history=truthy(row.get("history", "")) or (trendable and index <= 3),
            byte_order=row.get("byte_order", "0") or "0",
            word_order=row.get("word_order", "1") or "1",
            interval_time=row.get("interval_time", "1000") or "1000",
            value_strategy=row.get("value_strategy", "0") or "0",
        )
        points.append(point)
    return points


def attrs(element: ET.Element, **values: object) -> ET.Element:
    for key, value in values.items():
        element.set(key, str(value))
    return element


def extro(**values: object) -> str:
    return "".join(f"{key}::{value};" for key, value in values.items())


def add_chip(sys_data: ET.Element, chip_id: int, chip_type: int, extro_para: str, data_ids: list[int]) -> ET.Element:
    chip = attrs(ET.SubElement(sys_data, "CHIPV"), ID=chip_id, lock=0, Type=chip_type, Intvl=10, ExtroPara=extro_para)
    for data_id in data_ids:
        attrs(ET.SubElement(chip, "DeviceData"), DeviceID=1, DataID=data_id, ExtroPara="")
    return chip


def add_chip_bindings(sys_data: ET.Element, chip_id: int, chip_type: int, extro_para: str, bindings: list[tuple[int, int]]) -> ET.Element:
    chip = attrs(ET.SubElement(sys_data, "CHIPV"), ID=chip_id, lock=0, Type=chip_type, Intvl=10, ExtroPara=extro_para)
    for device_id, data_id in bindings:
        attrs(ET.SubElement(chip, "DeviceData"), DeviceID=device_id, DataID=data_id, ExtroPara="")
    return chip


def add_placement(page: ET.Element, chip_id: int, x: int, y: int, w: int, h: int) -> None:
    attrs(ET.SubElement(page, "CHIPV"), ID=chip_id, Width=w, Height=h, PosX=x, PosY=y)


def add_port(port_list: ET.Element, device: DeviceSpec) -> None:
    if device.port == "HOST":
        return
    if device.port.upper().startswith("COM"):
        trans_mode = {
            "dlt645": TRANS_MODE_DLT645,
            "cjt188": TRANS_MODE_CJT188,
            "dlt69845": TRANS_MODE_DLT69845,
        }.get(device.protocol, TRANS_MODE_MBRTU)
        com = attrs(
            ET.SubElement(port_list, "COM"),
            Name=device.port,
            Remarks="",
            Baund=device.serial_baud,
            Parity=device.serial_parity,
            StopBit=device.serial_stop_bit,
            DataBit=device.serial_data_bit,
            FlowCtrl=0,
            DTR=0,
            TransMode=trans_mode,
            BreakTime=10,
            DeviceType=device.device_type,
            Configured=1,
        )
        proto = ET.SubElement(com, "ProtocolPara")
        if device.protocol == "dlt645":
            attrs(proto, DLT645SendWake=device.dlt645_send_wake)
        elif device.protocol == "cjt188":
            attrs(proto, CJT188SendWake=device.cjt188_send_wake, CJT188AllowReversedDI=device.cjt188_allow_reversed_di)
        elif device.protocol == "dlt69845":
            attrs(proto, DLT69845CA=device.dlt69845_ca, DLT69845SendWake=device.dlt69845_send_wake)
        else:
            attrs(proto, CharType=0)
        return

    trans_mode = {
        "s7": TRANS_MODE_S7,
        "dlt645": TRANS_MODE_DLT645,
        "cjt188": TRANS_MODE_CJT188,
        "dlt69845": TRANS_MODE_DLT69845,
    }.get(device.protocol, TRANS_MODE_MBTCP_SYN)
    net = attrs(
        ET.SubElement(port_list, "NET"),
        cip="",
        Name=device.port,
        Remarks="",
        DesIP=device.ip,
        SrcPortID=0,
        LocalPortID=device.tcp_port,
        LocalIP="",
        SessionKey="",
        prxy=0,
        DesPortID=device.tcp_port,
        LinkMode=TCP_LINK_SERVER if device.device_type == DT_DEV_MBSLV else TCP_LINK_CLIENT,
        LinkResetTime=0,
        LinkHoldTime=6000,
        mintv=5,
        TransMode=trans_mode,
        DeviceType=device.device_type,
    )
    proto = ET.SubElement(net, "ProtocolPara")
    if device.protocol == "s7":
        attrs(proto, S7CpuModel=device.s7_cpu_model, S7LocalTSAP=0, S7RemoteTSAP=0, S7Rack=device.s7_rack, S7Slot=device.s7_slot, S7MaxPDU=960)
    elif device.protocol == "dlt645":
        attrs(proto, DLT645SendWake=device.dlt645_send_wake)
    elif device.protocol == "cjt188":
        attrs(proto, CJT188SendWake=device.cjt188_send_wake, CJT188AllowReversedDI=device.cjt188_allow_reversed_di)
    elif device.protocol == "dlt69845":
        attrs(proto, DLT69845CA=device.dlt69845_ca)
    else:
        attrs(proto, CharType=0, MaxAsynNum=15)


def add_demo_com_ports(port_list: ET.Element, used_ports: set[str]) -> None:
    for index in range(1, 5):
        name = f"COM{index}"
        if name in used_ports:
            continue
        com = attrs(
            ET.SubElement(port_list, "COM"),
            Name=name,
            Remarks="",
            Baund=9600,
            Parity=0,
            StopBit=0,
            DataBit=0,
            FlowCtrl=0,
            DTR=0,
            TransMode=TRANS_MODE_MBRTU,
            BreakTime=10,
            DeviceType=DT_DEV_MBMST,
            Configured=0,
        )
        attrs(ET.SubElement(com, "ProtocolPara"), CharType=0)


def build_project(points: list[Point], devices: list[DeviceSpec], device_name: str, pack_demo_style: bool) -> ET.ElementTree:
    root = ET.Element("MThings")
    port_list = ET.SubElement(root, "PORT_LIST")
    added_ports: set[str] = set()
    if pack_demo_style:
        add_demo_com_ports(port_list, {device.port for device in devices})
    for device in devices:
        if device.port not in added_ports:
            add_port(port_list, device)
            added_ports.add(device.port)
    attrs(ET.SubElement(root, "SIGNT"), SYS_DATA="", ALARM="", HISDATA="")
    ET.SubElement(root, "MQTT")

    curve = ET.SubElement(root, "CURVE")
    for point in points:
        if point.trend:
            device = next(d for d in devices if d.key == point.device_key)
            attrs(ET.SubElement(curve, "DATA"), DeviceID=device.device_id, DataID=point.data_id, IsRightY=0, Color=point.color, spsn=0)

    his = ET.SubElement(root, "HISDATA")
    for device in devices:
        device_history = [point for point in points if point.device_key == device.key and point.history]
        if not device_history:
            continue
        his_device = attrs(ET.SubElement(his, "DEVICE"), ID=device.device_id)
        for point in device_history:
            attrs(ET.SubElement(his_device, "DATA"), ID=point.data_id)

    alarm_list = ET.SubElement(root, "ALARM_LIST")
    ET.SubElement(alarm_list, "TYPELIST")
    sys_data = ET.SubElement(root, "SYS_DATA")
    page_width = 1920
    page_height = 920 if pack_demo_style else 1080
    overview = attrs(ET.SubElement(sys_data, "PAGE"), ID=1, Name="HOME" if pack_demo_style else "Overview", width=page_width, hight=page_height, BackColor="#ffffffff" if pack_demo_style else "#FFF4F7FB", PicPath="")
    detail = attrs(ET.SubElement(sys_data, "PAGE"), ID=2, Name="Page" if pack_demo_style else "Device Detail", width=page_width, hight=page_height, BackColor="#ff3c4450" if pack_demo_style else "#FFFFFFFF", PicPath="")

    add_placement(overview, 1, 32, 24, 620, 56)
    add_chip(sys_data, 1, 18, extro(text=f"{device_name} Overview", fsize=24, t_bd="ON", color="#FF111827", backcolor="#00000000"), [])

    analog = [p for p in points if p.show_type in (0, 2) and p.prtc_type in (0, 1, 2)]
    for offset, point in enumerate(analog[:4]):
        chip_id = 2 + offset
        x = 36 + (offset % 2) * 270
        y = 110 + (offset // 2) * 86
        add_placement(overview, chip_id, x, y, 250, 66)
        device = next(d for d in devices if d.key == point.device_key)
        add_chip_bindings(
            sys_data,
            chip_id,
            15,
            extro(
                bodercolor="#FF2563EB",
                boderwidth=1,
                rd_b=4,
                backcolor="#FFFFFFFF",
                tlname=point.name,
                fs_t=15,
                tlcolor="#FF1F2937",
                vlcolor="#FF111827",
                fs_v=20,
                preci=point.point_num,
                default=point.min_value,
                uint=point.unit,
            ),
            [(device.device_id, point.data_id)],
        )

    table_bindings = [(next(d.device_id for d in devices if d.key == p.device_key), p.data_id) for p in points[: min(len(points), 60)]]
    add_placement(overview, 6, 36, 330, 470, 260)
    add_chip_bindings(sys_data, 6, 0, extro(vcolmnHead="ON", uint="ON", vrange="ON", tlcolor="#FF111827", vlcolor="#FF111827", gdcolor="#FFE5E7EB", backcolor="#FFFFFFFF"), table_bindings)

    curve_bindings = [(next(d.device_id for d in devices if d.key == p.device_key), p.data_id) for p in points if p.trend][:3]
    if curve_bindings:
        add_placement(overview, 7, 540, 330, 520, 260)
        add_chip_bindings(sys_data, 7, 1, extro(tagcolor="#FF111827", cvcolor="#FFE74C3C", showlegend="ON", period=30, autoy="ON"), curve_bindings)

    command_points = [p for p in points if "W" in p.access and (p.show_type in (1, 7) or p.block in (0, 2))]
    if command_points:
        point = command_points[0]
        add_placement(overview, 8, 560, 110, 160, 58)
        device = next(d for d in devices if d.key == point.device_key)
        add_chip_bindings(sys_data, 8, 37, extro(bodercolor="#FF065F46", boderwidth=1, rd_b=4, backcolor="#FF10B981", tlcolor="#FFFFFFFF", fsize=18, text=point.name, openV=1, closeV=0, openN="ON", closeN="OFF"), [(device.device_id, point.data_id)])

    add_placement(overview, 9, 760, 110, 190, 58)
    add_chip(sys_data, 9, 22, extro(bodercolor="#FF374151", boderwidth=1, rd_b=4, backcolor="#FF4B5563", tlcolor="#FFFFFFFF", fsize=18, text="Device Detail", pageid=2), [])

    add_placement(detail, 10, 32, 24, 620, 56)
    add_chip(sys_data, 10, 18, extro(text=f"{device_name} Device Detail", fsize=24, t_bd="ON", color="#FF111827", backcolor="#00000000"), [])
    add_placement(detail, 11, 36, 110, 900, 420)
    add_chip_bindings(sys_data, 11, 0, extro(vcolmnHead="ON", uint="ON", vrange="ON", tlcolor="#FF111827", vlcolor="#FF111827", gdcolor="#FFE5E7EB", backcolor="#FFFFFFFF"), table_bindings)
    add_placement(detail, 12, 970, 110, 180, 58)
    add_chip(sys_data, 12, 22, extro(bodercolor="#FF374151", boderwidth=1, rd_b=4, backcolor="#FF4B5563", tlcolor="#FFFFFFFF", fsize=18, text="Back", pageid=1), [])

    attrs(ET.SubElement(root, "LOGIC_LIST"), ActiveID=0, ActiveIDs="")

    device_list = ET.SubElement(root, "DEVICE_LIST")
    for device_spec in devices:
        add_device(device_list, device_spec, [point for point in points if point.device_key == device_spec.key])

    ET.indent(root, space="\t")
    return ET.ElementTree(root)


def add_device(device_list: ET.Element, device_spec: DeviceSpec, points: list[Point]) -> None:
    base_attrs = dict(
        ID=device_spec.device_id,
        Name=device_spec.name,
        DeviceType=device_spec.device_type,
        BindMode=0,
        BindDeviceID=0,
        p_invl=device_spec.poll_interval,
        btime=0,
        BathReadMode=0,
        cGp=points[0].group if points else "Process",
    )
    if device_spec.protocol == "s7":
        base_attrs.update(S7MaxReadByte=960, S7MaxWriteByte=960, S7MaxReadItem=16, S7MaxWriteItem=16, S7PackWay=device_spec.s7_pack_way)
    elif device_spec.protocol == "dlt645":
        base_attrs.update(DLT645Addr=device_spec.dlt645_addr, DLT645IsBroadcastDevice=0, DLT645Password=device_spec.dlt645_password, DLT645Operator=device_spec.dlt645_operator)
    elif device_spec.protocol == "cjt188":
        base_attrs.update(CJT188Addr=device_spec.cjt188_addr)
    elif device_spec.protocol == "dlt69845":
        base_attrs.update(DLT69845SA=device_spec.dlt69845_sa)
    else:
        base_attrs.update(
            Addr=device_spec.modbus_addr or 1,
            mr_reg=125,
            mr_bit=2000,
            AddrUIMode=0,
            CRCByteOder=1,
            BlockBitOder=0,
            AddrOffset=0,
            PackWay=1,
            BitGap=0,
            RegGap=5,
            OneReg10=0,
            OneCoil0F=0,
            BCast1=0,
            BCast2=0,
            IsBCDevice=0,
        )
    device = attrs(ET.SubElement(device_list, "DEVICE"), **base_attrs)
    ports = ET.SubElement(device, "PORTS")
    attrs(ET.SubElement(ports, "port"), Name=device_spec.port)
    groups = ET.SubElement(device, "GROUPS")
    for group_name in sorted({p.group for p in points} or {"Process"}):
        attrs(ET.SubElement(groups, "group"), Name=group_name)
    ET.SubElement(device, "SELF_LIST")
    data_list = ET.SubElement(device, "DATA_LIST")
    for point in points:
        add_data(data_list, point)


def add_data(data_list: ET.Element, point: Point) -> None:
    data_attrs = dict(
        ID=point.data_id,
        Name=point.name,
        PrtcTYPE=point.prtc_type,
        PointNum=point.point_num,
        Unit=point.unit,
        ShowType=point.show_type,
        Gain=point.gain,
        Offset=point.offset,
        sz=point.byte_size,
        BitOffset=point.bit_offset,
        BitNum=point.bit_num,
        Range=point.range_text,
        CmdValue="",
        Color=point.color,
        ByteOder=point.byte_order,
        WordOder=point.word_order,
        IntervalTime=point.interval_time,
        IsBRead=1,
    )
    if point.protocol == "s7":
        data_attrs.update(S7Area=point.s7_area if point.s7_area is not None else 0, S7DBNo=point.s7_db_no, S7ByteOffset=point.s7_byte_offset)
    elif point.protocol == "dlt645":
        data_attrs.update(DLT645DI=point.dlt645_di if point.dlt645_di is not None else 0)
    elif point.protocol == "cjt188":
        data_attrs.update(CJT188DI=point.cjt188_di if point.cjt188_di is not None else 0, CJT188Offset=point.cjt188_data_offset)
    elif point.protocol == "dlt69845":
        data_attrs.update(
            DLT69845OI=point.dlt69845_oi if point.dlt69845_oi is not None else 0,
            DLT69845Attr=point.dlt69845_attr,
            DLT69845Index=point.dlt69845_index,
        )
    else:
        data_attrs.update(BLOCK=point.block, Addr=point.addr)
    data = attrs(ET.SubElement(data_list, "DATA"), **data_attrs)
    attrs(ET.SubElement(data, "TIMEOUT"), Time=2000, ResendCount=0)
    attrs(
        ET.SubElement(data, "VALUE"),
        Stategy=point.value_strategy,
        TimeBngrWay=0,
        Coef_a=0,
        Coef_b=0,
        Coef_c=0,
        Coef_d=0,
        Coef_e=0,
        ResetStep=0,
        XInvl=1000,
        MaxRadom=point.max_value,
        MinRadom=point.min_value,
        MaxValue=point.max_value,
        MinValue=point.min_value,
        datarry="",
        BindDeviceID=0,
        BindDataID=0,
    )
    enums = ET.SubElement(data, "ENUMS")
    for value, name in point.enum_items:
        attrs(ET.SubElement(enums, "enum"), Value=value, Name=name)
    groups = ET.SubElement(data, "GROUPS")
    attrs(ET.SubElement(groups, "group"), Name=point.group)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="Register table CSV/TSV/JSON")
    parser.add_argument("output", type=Path, help="Output .mthings file")
    parser.add_argument("--device-name", default="Protocol Device")
    parser.add_argument("--device-type", type=int, default=1, help="1 master/client, 2 slave/server, 4 local demo")
    parser.add_argument("--pack-demo-style", action="store_true", help="Mimic the packaged demo skeleton: COM1-COM4 placeholders, NET000 first network channel, and 1920x920 pages")
    args = parser.parse_args()

    rows = read_rows(args.input)
    normalized_rows = [
        row
        for row in rows
        if row.get("name")
        or row.get("address")
        or row.get("dlt645_di")
        or row.get("cjt188_di")
        or row.get("dlt69845_oi")
        or row.get("s7_byte_offset")
    ]
    devices = build_devices(normalized_rows, args.device_name, args.device_type, args.pack_demo_style)
    points = build_points(normalized_rows, devices)
    if not devices:
        raise SystemExit("No valid device rows found")
    tree = build_project(points, devices, args.device_name, args.pack_demo_style)
    tree.write(args.output, encoding="utf-8", xml_declaration=True)
    print(f"Generated {args.output} with {len(points)} data point(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
