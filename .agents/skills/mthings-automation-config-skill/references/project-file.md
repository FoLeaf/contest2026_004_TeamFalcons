# Complete `.mthings` Project File

## Purpose

Use this reference when generating a full MThings project file rather than only a `SYS_DATA` page/widget fragment.

## Section Order

Generated files should write sections under `<MThings>` in this order:

```xml
<MThings>
  <PORT_LIST />
  <SIGNT SYS_DATA="" ALARM="" HISDATA="" />
  <MQTT />
  <CURVE />
  <HISDATA />
  <ALARM_LIST />
  <SYS_DATA />
  <LOGIC_LIST ActiveID="0" ActiveIDs="" />
  <DEVICE_LIST />
</MThings>
```

Follow this order for generated files. Complete generated files should include explicit empty sections.

## Flow File

- Flow logic is stored in top-level `LOGIC_LIST`.
- `LOGIC_LIST` has `ActiveIDs`, a comma-separated list of active logic IDs. Empty means no active logic config.
- `ActiveID` is kept as a backward-compatible first-active ID.
- Each `LOGIC` child stores one flow-logic JSON object. Current `src_cfg` writes `Encoding="qbs"` with `qCompress(compact JSON, 9).toBase64()` text. Existing files without `Encoding` may still contain raw JSON CDATA and should remain readable.
- `.mlcc` files are flow-logic JSON used for export/import.
- Do not treat `.mlcc` as the same format as `.mthings`.

```xml
<LOGIC_LIST ActiveID="1" ActiveIDs="1,3">
  <LOGIC ID="1" Name="Main" Encoding="qbs">AAAAHXjaq1bKy09JLVayqq7VUUrOz8tLTS7JzM8DCkTH1gIAnKcKjQ==</LOGIC>
</LOGIC_LIST>
```

## Encoding

- MThings project files in the wild may omit an XML encoding declaration even when Chinese text is stored in the system ANSI code page. Try UTF-8 first, then GB18030/GBK before deciding a file is malformed.
- When creating a new project, prefer UTF-8 with an XML declaration. When patching an existing project, preserve its original encoding if it can be detected.

## Ports

Complete projects commonly include both serial and TCP ports:

```xml
<PORT_LIST>
  <COM Name="COM1" Remarks="" Baund="9600" Parity="0" StopBit="0" DataBit="0"
       FlowCtrl="0" DTR="0" TransMode="0" BreakTime="10" DeviceType="1" Configured="1">
    <ProtocolPara CharType="0" />
  </COM>
  <NET cip="" Name="NET001" Remarks="" DesIP="127.0.0.1" LocalPortID="502"
       LocalIP="" SessionKey="" DesPortID="502" LinkMode="0" LinkResetTime="0"
       LinkHoldTime="6000" TransMode="2" mintv="5" DeviceType="1">
    <ProtocolPara CharType="0" MaxAsynNum="15" />
  </NET>
</PORT_LIST>
```

- Use `<port Name="..."/>` or `<PORT Name="..."/>` under devices; both forms appear in saved projects, so validators should accept either.
- Serial channel names are physical OS serial port names stored as-is. Do not invent serial names for real hardware projects; use names such as `COM1`, `COM3`, or whatever the user/input supplies. `COM1`-`COM4` are acceptable only for synthetic/demo fixtures.
- Network channel names are logical MThings names. UI-created names use prefix `NET` plus a three-digit sequence: `NET001`, `NET002`, ... `NET999`.
- When adding a network channel, sort existing `NETnnn` names numerically, find the first numeric gap, and assign `NET%03d`. If no gaps exist, use max plus one. General new channels should start at `NET001`, but official packaged demo projects use `NET000` for the first TCP client and `NET001` for a TCP server; use that convention only when intentionally mimicking packaged demos or preserving an existing sample.
- Treat names with the `NET` prefix as network channels. Avoid custom network names that include `NET` in the middle.
- For a Modbus TCP master, create a `NETnnn` channel with `LinkMode="0"` (`TCP_LINK_CLIENT`), `DesIP` from the target, and `DesPortID="502"` unless specified.
- For a Modbus TCP slave/server, create a second NET-style port with `LinkMode="1"` (`TCP_LINK_SERVER`) and bind slave devices to it.
- `TransMode` values currently used by `src_cfg`: `0` Modbus RTU, `1` Modbus ASCII, `2` Modbus TCP sync, `3` Modbus TCP async, `4` Siemens S7, `5` DL/T645, `6` CJ/T188, `7` DL/T698.45.
- Store protocol-specific channel attributes in the `ProtocolPara` child: Modbus uses `CharType` and, for NET, `MaxAsynNum`; S7 uses `S7CpuModel`, `S7LocalTSAP`, `S7RemoteTSAP`, `S7Rack`, `S7Slot`, `S7MaxPDU`; DL/T645 uses `DLT645SendWake`; CJ/T188 uses `CJT188SendWake` and `CJT188AllowReversedDI`; DL/T698.45 uses `DLT69845CA` and, on serial channels, `DLT69845SendWake`.
- S7 is a single-device-per-port protocol. Generate one S7 device per S7 port unless patching an existing file that already follows another arrangement.
- S7 TCP defaults: `TransMode="4"`, `DesPortID="102"`, `LocalPortID="102"`, `LinkMode="0"`, `S7Rack="0"`, `S7Slot="2"`, `S7MaxPDU="960"`.
- DL/T645 defaults: `TransMode="5"`, serial baud commonly `2400`, even parity (`Parity="2"`), 8 data bits (`DataBit="0"`), 1 stop bit (`StopBit="0"`), network default port `20000`, and `DLT645SendWake="1"`.
- CJ/T188 defaults: `TransMode="6"`, serial baud `2400`, even parity (`Parity="2"`), 8 data bits (`DataBit="0"`), 1 stop bit (`StopBit="0"`), network port `20001`, `CJT188SendWake="2"`, and `CJT188AllowReversedDI="0"`.
- DL/T698.45 defaults: `TransMode="7"`, serial baud `2400`, even parity (`Parity="2"`), 8 data bits (`DataBit="0"`), 1 stop bit (`StopBit="0"`), network port `698`, `DLT69845CA="0"`, and serial `DLT69845SendWake="4"`.
- For a local computed/aggregate device, bind to `HOST` without needing a top-level port entry.
- `UDP_LINK_UNICAST` uses `LinkMode="2"` and must not duplicate another UDP channel's `LocalIP` + `LocalPortID` pair.
- Packaged demo projects commonly include `COM1` through `COM4` with `Configured="0"` even when those serial ports are unused. Keep or generate these placeholders only for demo fixtures; do not add them to real hardware projects unless requested.

## Device List

The full device section is:

```xml
<DEVICE_LIST>
  <DEVICE ID="1" Name="Demo PLC" DeviceType="4" BindMode="0" BindDeviceID="0" Addr="1"
          mr_reg="125" mr_bit="2000" p_invl="1" btime="0" AddrUIMode="0"
          CRCByteOder="1" BlockBitOder="0" AddrOffset="0" PackWay="0"
          BitGap="0" RegGap="0" OneReg10="0" OneCoil0F="0" BCast1="0"
          BCast2="0" IsBCDevice="0" BathReadMode="0" cGp="Process">
    <PORTS><port Name="HOST" /></PORTS>
    <GROUPS><group Name="Process" /></GROUPS>
    <SELF_LIST />
    <DATA_LIST>...</DATA_LIST>
  </DEVICE>
</DEVICE_LIST>
```

Common `DeviceType` values:

- `1`: master/client device
- `2`: slave/server device
- `4`: local/host device

For generated demo projects, prefer `DeviceType="4"` with `PORT Name="HOST"` so the file can stand alone.

For realistic automation demos, prefer this pattern:

- One or more master devices named like `[M]Station-1`, `DeviceType="1"`, bound to the same TCP-client port when they share a Modbus TCP target. Use `NET001` for general new projects or `NET000` for packaged-demo-style projects, unique `Addr` values, `PackWay="1"`, `RegGap="5"`, and `BathReadMode="2"`.
- Optional slave devices named like `[S]Station-1`, `DeviceType="2"`, bound to a TCP-server port such as `NET001` in packaged demos, with unique `Addr` values.
- Optional `HOST` device, `DeviceType="4"`, bound to `HOST`, for aggregate values or computed tags used by summary widgets.
- Keep `SELF_LIST` present even when empty because UI-saved projects include it.
- Modbus devices write `Addr`, `mr_reg`, `mr_bit`, `AddrUIMode`, `CRCByteOder`, `BlockBitOder`, `AddrOffset`, `PackWay`, `BitGap`, `RegGap`, `OneReg10`, `OneCoil0F`, `BCast1`, `BCast2`, and `IsBCDevice`.
- S7 devices write `S7MaxReadByte`, `S7MaxWriteByte`, `S7MaxReadItem`, `S7MaxWriteItem`, and `S7PackWay`. Defaults are `960`, `960`, `16`, `16`, and `1` (`S7_PW_EACH_ONE`).
- DL/T645 devices write `DLT645Addr` as 12 decimal digits, `DLT645IsBroadcastDevice`, `DLT645Password`, and `DLT645Operator`.
- CJ/T188 devices write `CJT188Addr` as a two-digit hexadecimal meter type followed by 14 decimal address digits.
- DL/T698.45 devices write `DLT69845SA` as 1-32 decimal digits with at least one non-zero digit.
- Generated devices use `p_invl="1"` unless an explicit device polling interval is supplied. `p_invl` is independent from data-point `IntervalTime`.

## Data Points

A typical register data point:

```xml
<DATA ID="1" Name="Temperature" BLOCK="2" PrtcTYPE="0" PointNum="1" Unit="C"
      ShowType="0" Addr="0" Gain="1" Offset="0" sz="2" BitOffset="0"
      BitNum="16" Range="0-100" CmdValue="" Color="#FFE74C3C"
      ByteOder="0" WordOder="1" IntervalTime="1000" IsBRead="1">
  <TIMEOUT Time="2000" ResendCount="0" />
  <VALUE Stategy="0" TimeBngrWay="0" Coef_a="0" Coef_b="0" Coef_c="0"
         Coef_d="0" Coef_e="0" ResetStep="0" XInvl="1000"
         MaxRadom="100" MinRadom="0" MaxValue="100" MinValue="0"
         datarry="" BindDeviceID="0" BindDataID="0" />
  <ENUMS />
  <GROUPS><group Name="Process" /></GROUPS>
</DATA>
```

Useful defaults:

- Current files write byte size as `sz`. Old files may contain `Size`, which the loader treats as register count and multiplies by 2. Prefer `sz` for newly generated files.
- Register `BLOCK`: `0` coils, `1` discrete inputs, `2` holding registers, `3` input registers.
- `PrtcTYPE`: `0` signed int, `1` unsigned int, `2` float, `3` bytes, `4` bit, `5` BCD, `6` signed BCD.
- `ShowType="0"` is float display; `ShowType="2"` is decimal integer in current enum ordering.
- `ByteOder="0"` and `WordOder="1"` match default big-byte/small-word order.
- `TIMEOUT Time="2000" ResendCount="0"`.
- `VALUE Stategy="0" TimeBngrWay="0" XInvl="1000"`.
- S7 points omit Modbus `BLOCK/Addr` and add `S7Area`, `S7DBNo`, `S7ByteOffset`; use `BitOffset` for bit-level tags. `S7Area` values are `0` DB, `1` M, `2` I, `3` Q, `4` counter, `5` timer.
- DL/T645 points omit Modbus `BLOCK/Addr` and add `DLT645DI`, the numeric data identifier. Preserve source DI notation in comments or intermediate tables when useful, but write the integer XML value.
- CJ/T188 points omit Modbus `BLOCK/Addr` and add integer `CJT188DI` plus byte-oriented `CJT188Offset`.
- DL/T698.45 points omit Modbus `BLOCK/Addr` and add integer `DLT69845OI`, `DLT69845Attr`, and `DLT69845Index`. Use `PrtcTYPE="7"` for self-described values when no scalar decoding is specified.

## Curves And History

Add curve metadata for each data point used by real-time or bar curve widgets:

```xml
<CURVE>
  <DATA DeviceID="1" DataID="1" IsRightY="0" Color="#FFE74C3C" spsn="0" />
</CURVE>
```

Add history entries when the project includes history curves or expected retention:

```xml
<HISDATA>
  <DEVICE ID="1">
    <DATA ID="1" />
  </DEVICE>
</HISDATA>
```

Packaged demos often add history for most station points, not only the points displayed on a curve. For complete demos, include all important analog/status points in `HISDATA`; for hardware-oriented projects, only add history when the point table or user asks for retention.

Use `ALARM_LIST` with an explicit empty type list when no alarm types are configured:

```xml
<ALARM_LIST>
  <TYPELIST />
</ALARM_LIST>
```

Generated alarms should follow the current serializer shape:

```xml
<ALARM ID="1" Name="Temperature High" Type="Process" Level="0" GenTime="0"
       ClearTime="0" Ralation12="0" Ralation23="0" Confirm="0" Sound="0">
  <OUT />
  <Condition1 isValid="true" DeviceID="1" DataID="1" Type="0" Th1="80" Th2=""
              Rd1="" Rd2="" isDes="false" desDv1="0" desData1="0" desDv2="0" desData2="0" />
  <Condition2 isValid="false" DeviceID="0" DataID="0" Type="0" Th1="" Th2=""
              Rd1="" Rd2="" isDes="false" desDv1="0" desData1="0" desDv2="0" desData2="0" />
  <Condition3 isValid="false" DeviceID="0" DataID="0" Type="0" Th1="" Th2=""
              Rd1="" Rd2="" isDes="false" desDv1="0" desData1="0" desDv2="0" desData2="0" />
</ALARM>
```

Alarm condition `Type` values: `0` greater than, `1` greater or equal, `2` equal, `3` not equal, `4` less or equal, `5` less than, `6` between, `7` outside range.

## Generation Strategy

For a useful standalone sample:

1. Create one local device with 4-8 data points.
2. Create an overview page and a detail page.
3. Use labels/cards for key values, a table for all values, a curve for trending values, and page-tab widgets for navigation.
4. Add `CURVE` and `HISDATA` sections for trend-related points.
5. Run `scripts/validate_mthings.py` before delivering the file.
