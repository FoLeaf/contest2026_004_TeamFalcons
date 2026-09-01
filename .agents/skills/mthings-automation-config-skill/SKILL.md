---
name: mthings-automation-config-skill
description: Generate or modify MThings .mthings project files from Modbus, Siemens S7, DL/T645, CJ/T188, DL/T698.45, or local device point tables/documents and SCADA widget requirements. Use when Codex needs to import protocol point definitions, add serial or network transport channels, normalize point tables, create channel/device/data configuration, generate pages, place widgets, bind device data to widgets, emit SYS_DATA XML, create CURVE/HISDATA/ALARM_LIST/DEVICE_LIST sections, or embed/export/import .mlcc logic control JSON.
---

# MThings Automation Config Skill

## Overview

Use this skill to generate complete MThings `.mthings` XML projects or individual SCADA pages/widgets from device/tag requirements. The skill captures the current `src_cfg` project-file structure: root sections, channels, protocol-specific devices, data points, pages, widget placements, widget definitions, bindings, alarms, history, curves, and embedded logic controls.

## Quick Workflow

1. Inspect the input: protocol point spreadsheet/CSV/TSV/JSON, extracted table from a document, existing `.mthings`, or user-provided point list. Treat `.mlcc` as flow-logic JSON, not the main project file. For existing `.mthings`, try UTF-8 first and then GB18030/GBK if Chinese text or XML parsing looks broken.
2. Read `references/modbus-import.md` for point-table column mapping, Modbus address normalization, and S7/DL/T645/CJ/T188/DL/T698.45 protocol fields.
3. Read `references/project-file.md` when generating a full `.mthings` file. Read `references/sys-data-model.md` when only generating or changing pages/widgets.
4. Run `scripts/generate_mthings_from_modbus.py <point-table> <output.mthings>` for CSV/TSV/JSON point tables, then refine the XML if the user requested custom screens, alarms, or logic. The script name is retained for compatibility but supports Modbus, S7, DL/T645, CJ/T188, DL/T698.45, and host rows.
   - Use `--pack-demo-style` when the target should resemble the packaged `mthings_cfg_demo.mthings` skeleton: placeholder `COM1`-`COM4`, first network client `NET000`, server `NET001` when needed, and `1920x920` pages.
5. Run `scripts/extract_chip_catalog.py <repo-root>` only when widget type IDs, default dimensions, max data bindings, or default extended parameters are needed and the catalog is not already known.
6. Choose pages and widgets from the available catalog.
7. Generate or patch configuration using the file-level relationship: add one top-level `SYS_DATA/CHIPV` widget definition, then add one or more `PAGE/CHIPV` placement nodes that reference that widget ID.
8. Validate with `scripts/validate_mthings.py <file.mthings>` and then check any remaining domain-specific assumptions manually.

## Generation Rules

- For a full project, create `<MThings>` with root sections in this order: `PORT_LIST`, `SIGNT`, `MQTT`, `CURVE`, `HISDATA`, `ALARM_LIST`, `SYS_DATA`, `LOGIC_LIST`, and `DEVICE_LIST`.
- Do not emit the old root flow-file-path attribute.
- Put `SYS_DATA` before `DEVICE_LIST`.
- For complete demos, include `PORT_LIST` entries for used ports. Use real OS serial names for hardware projects; only use synthetic `COM1`-`COM4` in demos. General generated projects should name new network channels as `NET%03d`, starting at `NET001` and filling numeric gaps; packaged-demo-style projects may use `NET000` for the first TCP client because the official sample does.
- Convert imported protocol rows into `DEVICE_LIST/DEVICE/DATA_LIST/DATA` first, then derive pages/widgets/curves/history/alarms from those normalized `DeviceID/DataID` pairs.
- Support multiple devices. Device IDs scope their data IDs; every widget binding must carry the right `DeviceID` and `DataID`.
- Preserve original point/register names in `DATA Name`, units in `Unit`, ranges in `Range`, groups in `GROUPS`, enum definitions in `ENUMS`, writable values in command-capable widgets, and protocol address fields such as Modbus `BLOCK/Addr`, S7 `S7Area/S7DBNo/S7ByteOffset`, DL/T645 `DLT645DI`, CJ/T188 `CJT188DI/CJT188Offset`, or DL/T698.45 `DLT69845OI/DLT69845Attr/DLT69845Index`.
- Set the generated device polling interval `p_invl` to `1` by default. Override it only from an explicit `device_poll_interval`, `device_polling_interval`, or `p_invl` input column; do not confuse it with per-point `IntervalTime`.
- Create at least one page. If none exists, use page ID `1`, name `HOME`, size `1920x1080`, and background `#FFFFFFFF`. Use `1920x920` when intentionally mimicking UI-saved desktop demo projects.
- Store each widget twice: placement under `SYS_DATA/PAGE/CHIPV`, full widget definition under top-level `SYS_DATA/CHIPV`.
- A widget definition can be placed on several pages; this is normal for page tabs/navigation. Do not duplicate the top-level widget definition for each placement.
- Pack widget parameters as `id::value;id::value;`. Keep unknown parameter IDs intact when modifying existing files.
- Bind widget data with child `DeviceData` nodes containing `DeviceID`, `DataID`, and optional per-binding `ExtroPara`.
- Define every bound `DeviceID/DataID` under `DEVICE_LIST/DEVICE/DATA_LIST/DATA`, even for sample or simulated projects.
- Add `CURVE/DATA` entries for curve widgets and `HISDATA/DEVICE/DATA` entries when history charts or retained history are expected.
- For official-demo-like multi-station Modbus projects, prefer 3 pages: common widgets/control, data instruments, and trend display. Reuse the same page-tab `CHIPV` definitions across pages instead of duplicating them.
- For station overview screens, add device-state widgets bound to representative station data, cards for key analog values, command widgets for writable setpoints, and a `HOST` aggregate device when summary values such as total power are requested.
- Add `<ALARM_LIST><TYPELIST /></ALARM_LIST>` for complete projects with no configured alarm types. When alarms are generated, each `ALARM` should contain `OUT`, `Condition1`, `Condition2`, `Condition3`, and `TYPELIST` should include used alarm type names.
- Default widget refresh interval is `10`.
- Prefer catalog default dimensions; override only when the user asks for a layout or when dense placement requires it.
- Assign new page, widget, device, and data IDs as max existing numeric ID plus 1 unless the user provides explicit IDs.

## Widget Selection

Use this mapping when the user gives an intent rather than an exact widget type:

- Single numeric value: `CTEN_LABEL`, `CTEN_DATA_PANEL`, `CTEN_GAUGE`, `CTEN_GAUGE_SPEED`, `CTEN_PROGR_1`, `CTEN_PROGR_2`, `CTEN_PROGR_3`, `CTEN_TEMP`, `CTEN_BATTER`, `CTEN_RING`.
- Many values/table: `CTEN_TBL`.
- Real-time/history/day curves: `CTEN_CURV_R`, `CTEN_CURV_H`, `CTEN_CURV_D`, `CTEN_BAR_CRV`.
- Switch/output command: `CTEN_SWITCH`, `CTEN_BTN_SWITCH`, `CTEN_CMD_BTN`, `CTEN_MULCMD`, `CTEN_RESET_BTN`.
- State display: `CTEN_STATE_LIGHT`, `CTEN_MUL_TAG`, `CTEN_DEVICE_SM`, `CTEN_PIC_SWITCH`.
- Navigation/page jump: `CTEN_PAGE_TAB`, `CTEN_BTN_LINK`.
- Static text/image/line/flow: `CTEN_TEXT`, `CTEN_PIC_PANEL`, `CTEN_SPLIT`, `CTEN_FLOW_PATH`.

Load `references/widget-generation.md` when tuning extended parameters for common widgets.

## XML Shape

Generate this shape inside `<MThings>`:

```xml
<SYS_DATA>
  <PAGE ID="1" Name="HOME" width="1920" hight="1080" BackColor="#FFFFFFFF" PicPath="">
    <CHIPV ID="1" Width="200" Height="50" PosX="20" PosY="20" />
  </PAGE>
  <CHIPV ID="1" lock="0" Type="15" Intvl="10" ExtroPara="tlname::Temperature;uint::C;">
    <DeviceData DeviceID="1" DataID="1" ExtroPara="" />
  </CHIPV>
</SYS_DATA>
```

Attribute names intentionally preserve local spellings such as `hight`, `Intvl`, and `ExtroPara`.

## Validation Checklist

- XML parses and root is `MThings`.
- All `PAGE/CHIPV` placement IDs have matching top-level `SYS_DATA/CHIPV` definitions.
- All widget `DeviceData DeviceID/DataID` pairs exist under `DEVICE_LIST`.
- Device `PORTS` entries refer to `HOST` or to a top-level `PORT_LIST` `COM`/`NET` name. Accept both `<port>` and `<PORT>` child spellings. Network channel names should be `NET` plus three digits unless preserving an existing file.
- No widget exceeds its catalog max data-binding count.
- All placements fit within their page `width`/`hight`.
- Navigation widgets point to existing page IDs through `pageid`.
- Empty sections are explicit when unused, for example `<MQTT />`, `<ALARM_LIST />`, and `<PORT_LIST />`.

## Protocol Point Import

For spreadsheets, CSV/TSV, JSON, or protocol tables extracted from PDF/DOCX:

- Normalize the source table into rows with at least `name` and `address` or `addr`.
- Use `protocol` when available. Accepted values include `modbus`, `s7`, `siemens`, `dlt645`, `dl/t645`, `cjt188`, `cj/t188`, `dlt69845`, `dl/t698.45`, `host`, and `local`. If omitted, infer from protocol-specific columns before falling back to Modbus.
- Prefer these optional columns when available: `block`, `function`, `data_type`, `unit`, `scale`/`gain`, `offset`, `quantity`/`size`, `decimals`, `min`, `max`, `access`/`rw`, `group`, `enum`, `alarm_hi`, `alarm_lo`.
- Infer Modbus block from address prefixes when no block is supplied: `0xxxx` coils, `1xxxx` discrete inputs, `3xxxx` input registers, `4xxxx` holding registers.
- For S7 rows, prefer `s7_area`, `s7_db_no`, `s7_byte_offset`, `s7_bit_offset`, `data_type`, and optional `s7_rack`/`s7_slot` channel hints.
- For DL/T645 rows, prefer `dlt645_addr`, `dlt645_di`, `data_type`, `unit`, `point`, and optional `dlt645_password`/`dlt645_operator`.
- For CJ/T188 rows, prefer `cjt188_addr`, `cjt188_meter_type`, `cjt188_di`, `cjt188_data_offset`, and optional `cjt188_send_wake`/`cjt188_allow_reversed_di`.
- For DL/T698.45 rows, prefer `dlt69845_sa`, `dlt69845_oi`, `dlt69845_attr`, `dlt69845_index`, and optional `dlt69845_ca`/`dlt69845_send_wake`.
- Use `references/modbus-import.md` for exact mapping rules and current enum values.
- For document imports, extract tables first with the appropriate document/spreadsheet tooling, save a CSV/TSV/JSON intermediate, then run the generator.

## Resources

- `scripts/extract_chip_catalog.py`: Extract widget type values, default sizes, max bindings, and default extended parameters from the local widget catalog.
- `scripts/generate_mthings_from_modbus.py`: Generate a complete `.mthings` project from Modbus, S7, DL/T645, CJ/T188, DL/T698.45, or host CSV/TSV/JSON point tables, including channels, device data, overview/detail pages, table/cards/curve widgets, and curve/history metadata.
- `scripts/validate_mthings.py`: Validate generated `.mthings` XML structure, page/widget consistency, data bindings, page bounds, and common navigation mistakes.
- `templates/dlt645_2007_common.dtmthings`: Importable DL/T645-2007 common meter data template grouped by energy, instantaneous values, demand, and time/communication points. It uses standard-neutral `tariff1`-`tariff4` naming instead of assuming a meter's tariff labels map to sharp/peak/flat/valley. Treat it as a practical starting point and verify supported DI items/scaling against the actual meter manual.
- `references/modbus-import.md`: Register-table input schema, column aliases, Modbus address/block/type mapping, and import workflow.
- `references/project-file.md`: Complete `.mthings` project structure, root sections, device/data XML, and file-level generation guidance.
- `references/sys-data-model.md`: `SYS_DATA` XML shape, page/widget relationships, and validation rules.
- `references/widget-generation.md`: Widget catalog usage and practical generation heuristics.
