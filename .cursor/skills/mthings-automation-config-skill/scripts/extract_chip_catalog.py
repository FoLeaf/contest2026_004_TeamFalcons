#!/usr/bin/env python3
"""Extract MThings chip catalog data from src_chip/chipdef.*."""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//.*", "", text)


def parse_enum(header: str) -> dict[str, int]:
    match = re.search(r"enum\s+CHIP_TYPE_EN\s*\{(?P<body>.*?)\};", header, re.S)
    if not match:
        return {}

    values: dict[str, int] = {}
    current = -1
    for raw in match.group("body").split(","):
        item = raw.strip()
        if not item:
            continue
        if "=" in item:
            name, value = [part.strip() for part in item.split("=", 1)]
            current = int(value, 0)
        else:
            name = item
            current += 1
        values[name] = current
    return values


def split_top_level_items(body: str) -> list[str]:
    items: list[str] = []
    depth = 0
    start = None
    for index, char in enumerate(body):
        if char == "{":
            if depth == 0:
                start = index + 1
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0 and start is not None:
                items.append(body[start:index].strip())
                start = None
    return items


def split_csv_fields(item: str) -> list[str]:
    fields: list[str] = []
    current: list[str] = []
    in_string = False
    escape = False
    for char in item:
        if in_string:
            current.append(char)
            if escape:
                escape = False
            elif char == "\\":
                escape = True
            elif char == '"':
                in_string = False
        elif char == '"':
            in_string = True
            current.append(char)
        elif char == ",":
            fields.append("".join(current).strip())
            current = []
        else:
            current.append(char)
    tail = "".join(current).strip()
    if tail:
        fields.append(tail)
    return fields


def parse_extro_arrays(source: str) -> dict[str, list[dict[str, str]]]:
    arrays: dict[str, list[dict[str, str]]] = {}
    pattern = re.compile(
        r"const\s+CHIP_EXTR_PARA_S\s+(?P<name>\w+)\[\]\s*=\s*\{(?P<body>.*?)\};",
        re.S,
    )
    for match in pattern.finditer(source):
        params: list[dict[str, str]] = []
        for item in split_top_level_items(match.group("body")):
            fields = split_csv_fields(item)
            if len(fields) < 7:
                continue
            params.append(
                {
                    "type": fields[0],
                    "enum_type": fields[1],
                    "id": fields[2],
                    "name_lang": fields[3],
                    "default": fields[4].strip('"'),
                    "max": fields[5].strip('"'),
                    "min": fields[6].strip('"'),
                }
            )
        arrays[match.group("name")] = params
    return arrays


def parse_chip_map(source: str, enum_values: dict[str, int], extro_arrays: dict[str, list[dict[str, str]]]) -> list[dict[str, object]]:
    match = re.search(r"const\s+CHIP_GEN_MAP_S\s+CAST_CHIP_GEN_INFO\[\]\s*=\s*\{(?P<body>.*?)\};", source, re.S)
    if not match:
        return []

    catalog: list[dict[str, object]] = []
    for item in split_top_level_items(match.group("body")):
        fields = split_csv_fields(item)
        if len(fields) < 7:
            continue
        enum_name = fields[0]
        extro_name = fields[3]
        catalog.append(
            {
                "enum": enum_name,
                "type_id": enum_values.get(enum_name),
                "factory": fields[1],
                "max_data_num": int(fields[2], 0),
                "extro_array": None if extro_name == "0" else extro_name,
                "default_width": int(fields[5], 0),
                "default_height": int(fields[6], 0),
                "extro_defaults": extro_arrays.get(extro_name, []),
            }
        )
    return catalog


def main() -> int:
    repo = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    chipdef_h = repo / "src_chip" / "chipdef.h"
    chipdef_cpp = repo / "src_chip" / "chipdef.cpp"

    if not chipdef_h.exists() or not chipdef_cpp.exists():
        print(f"Missing src_chip/chipdef.h or src_chip/chipdef.cpp under {repo}", file=sys.stderr)
        return 2

    header = strip_comments(chipdef_h.read_text(encoding="utf-8-sig", errors="replace"))
    source = strip_comments(chipdef_cpp.read_text(encoding="utf-8-sig", errors="replace"))
    enum_values = parse_enum(header)
    extro_arrays = parse_extro_arrays(source)
    catalog = parse_chip_map(source, enum_values, extro_arrays)
    print(json.dumps(catalog, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
