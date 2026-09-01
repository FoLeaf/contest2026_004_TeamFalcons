# Protocol Point Import

## Goal

Use this reference when the user provides a Modbus register table, Siemens S7 point table, DL/T645 meter table, CJ/T188 meter table, DL/T698.45 object table, local tag list, or extracted device manual table and expects a complete `.mthings` project with channels, device data, screens, trends/history, alarms, and optional logic.

## Input Workflow

1. If the source is `.xlsx`, `.csv`, `.tsv`, or JSON, load it directly.
2. If the source is PDF/DOCX/manual text, extract the register table first and save a CSV/TSV/JSON intermediate.
3. Normalize column names and units.
4. Generate `.mthings` with `scripts/generate_mthings_from_modbus.py`. The script name is historical; it now supports Modbus, S7, DL/T645, CJ/T188, DL/T698.45, and host rows.
5. Validate with `scripts/validate_mthings.py`.
6. Patch the generated XML for custom alarm or logic requirements the table could not express.

## Recommended Columns

Minimum:

- `name`: point name
- `address`, `addr`, `s7_byte_offset`, `dlt645_di`, `cjt188_di`, or `dlt69845_oi`: protocol address

Preferred optional columns:

- `protocol`: `modbus`, `s7`, `siemens`, `dlt645`, `dl/t645`, `cjt188`, `cj/t188`, `dlt69845`, `dl/t698.45`, `host`, or `local`
- `device`, `device_id`, `device_type`: device grouping and MThings device metadata
- `port`: device port binding such as `NET001`, `COM1`, or `HOST`
- `ip`, `tcp_port`, `serial_baud`, `serial_parity`, `serial_data_bit`, `serial_stop_bit`: channel metadata
- `station` or `slave_addr`: Modbus station address
- `block`, `register_type`, or `function`: coil/discrete/input/holding
- `s7_area`, `s7_db_no`, `s7_byte_offset`, `s7_bit_offset`, `s7_rack`, `s7_slot`, `s7_cpu_model`, `s7_pack_way`: Siemens S7 metadata
- `dlt645_addr`, `dlt645_di`, `dlt645_password`, `dlt645_operator`, `dlt645_send_wake`: DL/T645 metadata
- `cjt188_addr`, `cjt188_meter_type`, `cjt188_di`, `cjt188_data_offset`, `cjt188_send_wake`, `cjt188_allow_reversed_di`: CJ/T188 metadata
- `dlt69845_sa`, `dlt69845_oi`, `dlt69845_attr`, `dlt69845_index`, `dlt69845_ca`, `dlt69845_send_wake`: DL/T698.45 metadata
- `data_type`: bool, bit, uint16, int16, uint32, int32, float32, float, string, enum
- `quantity` or `size`: Modbus register count or byte-oriented protocol size hint
- `unit`
- `scale` or `gain`
- `offset`
- `decimals` or `point`
- `min`, `max`, or `range`
- `access` or `rw`: R, W, RW, RO
- `group`
- `enum`: value labels, for example `0:OFF;1:ON`
- `alarm_hi`, `alarm_lo`: optional alarm thresholds
- `trend` or `history`: true/false hints
- `byte_order`, `word_order`, `poll_interval`, `pack_way`, `bath_read_mode`: advanced point communication/data defaults
- `device_poll_interval`, `device_polling_interval`, or `p_invl`: device polling interval; default `1`

## Column Aliases

The generator accepts common aliases:

- name: `name`, `tag`, `point`, `signal`, `description`, `desc`, `variable`
- address: `address`, `addr`, `register`, `reg`, `offset`
- block: `block`, `area`, `register_type`, `reg_type`, `function`, `func`, `fc`
- data type: `data_type`, `type`, `datatype`, `value_type`
- unit: `unit`, `units`
- scale/gain: `scale`, `gain`, `factor`
- decimals: `decimals`, `point`, `precision`
- access: `access`, `rw`, `read_write`

## Protocol Detection

- Prefer an explicit `protocol` column; when present, it wins over any inferred clue from address or protocol-specific columns.
- Infer S7 when `s7_area`, `s7_db_no`, `s7_byte_offset`, or S7 wording appears in the row.
- Infer DL/T645 when `dlt645_di`, `dlt645_addr`, `645`, or meter-address wording appears in the row.
- Infer CJ/T188 when `cjt188_di`, `cjt188_addr`, or `188` wording appears in the row.
- Infer DL/T698.45 when `dlt69845_oi`, `dlt69845_sa`, or `698` wording appears in the row.
- Infer host/local when the port or protocol is `HOST`, `host`, or `local`.
- Fall back to Modbus for generic register tables.

## Channel Generation

- Modbus TCP: `NETnnn`, `TransMode="2"`, `LinkMode="0"`, default port `502`, and `<ProtocolPara CharType="0" MaxAsynNum="15" />`.
- Modbus RTU/ASCII: use explicit real `COMx` ports for hardware; `TransMode="0"` for RTU or `1` for ASCII, with `<ProtocolPara CharType="0" />`.
- S7: `NETnnn`, `TransMode="4"`, default port `102`, `LinkMode="0"`, and `<ProtocolPara S7CpuModel="0" S7LocalTSAP="0" S7RemoteTSAP="0" S7Rack="0" S7Slot="2" S7MaxPDU="960" />`. Generate one S7 device per port.
- DL/T645 serial: usually `TransMode="5"`, baud `2400`, even parity (`Parity="2"`), 8 data bits (`DataBit="0"`), 1 stop bit (`StopBit="0"`), and `<ProtocolPara DLT645SendWake="1" />`.
- DL/T645 network: `NETnnn`, `TransMode="5"`, default port `20000`, `LinkMode="0"` unless the requirement is a local server.
- CJ/T188 serial: `TransMode="6"`, default baud `2400`, even parity (`Parity="2"`), 8 data bits (`DataBit="0"`), 1 stop bit (`StopBit="0"`), and `<ProtocolPara CJT188SendWake="2" CJT188AllowReversedDI="0" />`.
- CJ/T188 network: `NETnnn`, `TransMode="6"`, default port `20001`, and the same CJ/T188 protocol parameters.
- DL/T698.45 serial: `TransMode="7"`, default baud `2400`, even parity (`Parity="2"`), 8 data bits (`DataBit="0"`), 1 stop bit (`StopBit="0"`), and `<ProtocolPara DLT69845CA="0" DLT69845SendWake="4" />`.
- DL/T698.45 network: `NETnnn`, `TransMode="7"`, default port `698`, and `<ProtocolPara DLT69845CA="0" />`.
- Host/local tags: use `PORTS/port Name="HOST"` and do not add a top-level port entry.
- Supply `ip` for real network devices. The generator uses `127.0.0.1` only as a placeholder when it is omitted.
- When generating a project intended to look like the packaged `mthings_cfg_demo.mthings`, use `scripts/generate_mthings_from_modbus.py --pack-demo-style`. That profile keeps unused `COM1`-`COM4` placeholders, starts generated network channels at `NET000`, and uses `1920x920` pages.

## Multiple Devices

- Group rows by `device` or `device_id` when present. Assign stable device IDs from the table; otherwise allocate IDs in first-seen order.
- For `device_type`, accept `master`, `modbus_master`, or `1`; `slave`, `modbus_slave`, or `2`; `host`, `local`, or `4`.
- Bind master devices to the first available `NET%03d` channel by default, slave devices to the next `NET%03d` channel, and host devices to `HOST`.
- Add top-level `PORT_LIST` entries when using NET or COM ports. Preserve explicit `port` values from the table. For generated hardware projects, use the real serial names supplied by the table/user; for demo-only projects, synthetic `COM1`-`COM4` are acceptable.
- Data IDs are scoped per device. A widget binding must use both the target `DeviceID` and the row's per-device `DataID`.
- S7 is single-device-per-port in the current protocol descriptor. Do not place multiple S7 devices on the same generated S7 channel.
- DL/T645 devices are addressed by `DLT645Addr`, a 12-digit decimal meter address. If the user supplies a short numeric address, left-pad it to 12 digits.
- CJ/T188 devices are addressed by `CJT188Addr`: a two-digit hexadecimal meter type followed by 14 decimal address digits. Default the meter type to `10H`, left-pad a short meter address, and reject the all-99 broadcast address for a normal device.
- DL/T698.45 devices are addressed by `DLT69845SA`, containing 1-32 decimal digits and at least one non-zero digit. Use `000000000001` when no SA is supplied.

## Demo Layout Pattern

When the user asks for a complete demo rather than a minimal import:

- Use three pages when enough data exists: overview/control, data dashboard, and trend page. Packaged demos name the first page `HOME` and commonly use `Page` for follow-up pages.
- Reuse page-tab widgets on each page.
- Use a table per major device, device state widgets for stations, command widgets for writable enum/boolean points, and a host aggregate widget when computed totals are requested.
- Add real-time, history, and day/bar curve widgets when trend/history columns are present or when several analog points exist.
- For multi-station Modbus demos, it is acceptable for several master devices with different slave addresses to share one TCP-client channel, as shown by `[M]Station-1`, `[M]Station-2`, and `[M]Station-3` sharing `NET000` in the packaged sample.

## Address And Block Mapping

When a block is not explicit, infer from address prefixes:

- `00001`-style or `0xxxx`: coils, `BLOCK=0`
- `10001`-style or `1xxxx`: discrete inputs, `BLOCK=1`
- `30001`-style or `3xxxx`: input registers, `BLOCK=3`
- `40001`-style or `4xxxx`: holding registers, `BLOCK=2`

MThings stores the zero-based Modbus address in `Addr`. For example:

- `40001` -> `BLOCK=2`, `Addr=0`
- `30010` -> `BLOCK=3`, `Addr=9`
- `00005` -> `BLOCK=0`, `Addr=4`

If the table already uses zero-based offsets and supplies `block`, keep the numeric address as-is.

## S7 Mapping

- `S7Area`: `0` DB, `1` M, `2` I, `3` Q, `4` counter, `5` timer.
- `S7DBNo`: DB number; set `0` for non-DB areas.
- `S7ByteOffset`: zero-based byte offset. Use `BitOffset` for boolean tags such as `DB1.DBX10.3`.
- S7 data rows should omit Modbus `BLOCK` and `Addr`.
- S7 device defaults: `S7MaxReadByte="960"`, `S7MaxWriteByte="960"`, `S7MaxReadItem="16"`, `S7MaxWriteItem="16"`, `S7PackWay="1"`. Use `S7PackWay="0"` only when the user wants automatic range merging.

## DL/T645 Mapping

- Device address: write `DLT645Addr` as 12 decimal digits. Preserve explicit `DLT645Password` and `DLT645Operator` for write-capable meter points.
- Data identifier: write `DLT645DI` as an integer XML attribute. Accept source values such as `00010000`, `0x00010000`, or `00-01-00-00` and normalize them before writing.
- DL/T645 data rows should omit Modbus `BLOCK` and `Addr`.
- DL/T645 commonly uses BCD-like numeric payloads. Prefer `PrtcTYPE=5` or `6` when the manual says BCD; otherwise use integer/float mapping from the point table.

## CJ/T188 Mapping

- Write the canonical device identity to `CJT188Addr` as meter type plus meter address, for example `1000000000000001`.
- Write the two-byte data identifier to `CJT188DI` as an integer and the byte position after DI/SER to `CJT188Offset`.
- Omit Modbus `BLOCK/Addr`. Treat `quantity`/`size` as a byte count and keep `CJT188Offset + sz <= 252`.
- Use BCD or signed BCD types when required by the meter definition. `CJT188AllowReversedDI` is a compatibility switch and defaults to `0`.

## DL/T698.45 Mapping

- Write the server address to `DLT69845SA`; write the client address to channel `ProtocolPara/DLT69845CA`.
- Encode each OAD with integer attributes `DLT69845OI`, `DLT69845Attr`, and `DLT69845Index`. Attribute number is limited to `0-31`; index is limited to `0-255`, where `0` means the whole attribute.
- Omit Modbus `BLOCK/Addr`. Treat `quantity`/`size` as a byte count.
- Use `PrtcTYPE=7` (`STT_SELF_DESC`) when the table does not prescribe a decoded scalar type; DL/T698.45 response data is self-described.

## Type Mapping

MThings stores protocol and display types as numeric values:

- `PrtcTYPE`: `STT_INT=0`, `STT_UINT=1`, `STT_FLOAT=2`, `STT_BYTES=3`, `STT_BIT=4`
- Current additional `PrtcTYPE` values: `STT_BCD=5`, `STT_BCD_S=6`, `STT_SELF_DESC=7`
- `ShowType`: `SST_FLOAT=0`, `SST_INT_BIT=1`, `SST_INT_DEC=2`, `SST_INT_HEX=3`, `SST_BYTES=4`, `SST_STRING=5`, `SST_TIME=6`, `SST_ENUM=7`

Practical mapping:

- bool/coil/bit: `PrtcTYPE=4`, `ShowType=1`, `sz=2` for Modbus bit areas or `sz=1` for S7 bit tags, `BitNum=1`
- uint16: `PrtcTYPE=1`, `ShowType=2`, `sz=2`
- int16: `PrtcTYPE=0`, `ShowType=2`, `sz=2`
- uint32/int32/float32: `sz=4`; use `PrtcTYPE=2` for float32, otherwise integer type
- float64/double: `PrtcTYPE=2`, `ShowType=0`, `sz=8`, `BitNum=64`
- enum: integer protocol type with `ShowType=7` and populated `ENUMS`
- string/bytes: `PrtcTYPE=3`, `ShowType=5` for string or `4` for bytes

## Screen Generation Heuristics

- Build an overview page with title, first 4 key analog cards, a table of all points, a trend curve for first 1-3 trendable analog points, and navigation.
- Build a detail page with a full table and command/state controls.
- Use command-capable widgets for writable boolean/enum points.
- Add `CURVE/DATA` and `HISDATA` for analog points marked trend/history or the first few analog points when no hint is present.
- Generate `ALARM_LIST/ALARM` when `alarm_hi`, `alarm_lo`, or explicit alarm rules exist. Use `Type` as the alarm class name, `Level=0` for major/high by default, `Condition1 Type=0` for high alarms, and `Condition1 Type=5` for low alarms unless the source says inclusive.

## Logic Control

Flow logic is embedded in the `.mthings` file under `LOGIC_LIST`. `.mlcc` is flow-logic JSON used for export/import.

Generate or patch `.mlcc` only when the user requests logic such as:

- interlock
- alarm linkage
- computed tags
- pump start/stop sequence
- PID-like control
- timer or script logic

For pure register imports, emit `<LOGIC_LIST ActiveID="0" ActiveIDs="" />`.
