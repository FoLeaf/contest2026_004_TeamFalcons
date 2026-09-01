#!/usr/bin/env python3
"""Extend velaguard.mthings RS485 slave bus to 32 Modbus slave devices."""

from __future__ import annotations

import copy
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

TARGET = Path("/mnt/c/Users/19y/Documents/mthings/velaguard.mthings")
PORT = "COM6"

COLORS = [
    "#FF3498DB", "#FFE74C3C", "#FF6366F1", "#FF10B981", "#FFF59E0B", "#FF8B5CF6",
    "#FF2ECC71", "#FFEC4899", "#FF14B8A6", "#FFF97316", "#FF84CC16", "#FF06B6D4",
]

# (device_id, slave_addr, name, group, points)
# point: (name, unit, range, gain, show_type, prtc_type, mock_strategy, mock_params, enums?)
NEW_DEVICES = [
    (4, 4, "[S]烟雾-SD100", "消防", [
        ("烟雾浓度", "%obs/m", "0-100", "0.1", 0, 1, 3, dict(Coef_a=1, Coef_b=0.01, Coef_c=15, MaxRadom=5, MinRadom=0, MaxValue=100, MinValue=0)),
    ]),
    (5, 5, "[S]CO2-MH410", "环境", [
        ("CO2浓度", "ppm", "0-5000", "1", 2, 1, 3, dict(Coef_a=1, Coef_b=1, Coef_c=650, MaxRadom=80, MinRadom=0, MaxValue=5000, MinValue=0)),
    ]),
    (6, 6, "[S]PM2.5-AQ100", "环境", [
        ("PM2.5", "ug/m3", "0-500", "0.1", 0, 1, 3, dict(Coef_a=1, Coef_b=0.1, Coef_c=35, MaxRadom=8, MinRadom=0, MaxValue=500, MinValue=0)),
        ("PM10", "ug/m3", "0-600", "0.1", 0, 1, 3, dict(Coef_a=1, Coef_b=0.1, Coef_c=48, MaxRadom=10, MinRadom=0, MaxValue=600, MinValue=0)),
    ]),
    (7, 7, "[S]光照-LS200", "环境", [
        ("照度", "lux", "0-200000", "1", 2, 1, 3, dict(Coef_a=2, Coef_b=10, Coef_c=450, MaxRadom=200, MinRadom=0, MaxValue=200000, MinValue=0)),
    ]),
    (8, 8, "[S]噪声-NS300", "环境", [
        ("噪声", "dB", "30-120", "0.1", 0, 1, 3, dict(Coef_a=1, Coef_b=0.1, Coef_c=55, MaxRadom=3, MinRadom=0, MaxValue=120, MinValue=30)),
    ]),
    (9, 9, "[S]气压-BP100", "环境", [
        ("气压", "hPa", "800-1100", "0.1", 0, 1, 3, dict(Coef_a=0, Coef_b=0.1, Coef_c=1013, MaxRadom=2, MinRadom=0, MaxValue=1100, MinValue=800)),
    ]),
    (10, 10, "[S]风速-WS500", "气象", [
        ("风速", "m/s", "0-60", "0.1", 0, 1, 3, dict(Coef_a=1, Coef_b=0.1, Coef_c=35, MaxRadom=5, MinRadom=0, MaxValue=60, MinValue=0)),
        ("风向", "deg", "0-360", "1", 2, 1, 3, dict(Coef_a=2, Coef_b=1, Coef_c=180, MaxRadom=20, MinRadom=0, MaxValue=360, MinValue=0)),
    ]),
    (11, 11, "[S]液位-LT400", "过程", [
        ("液位", "%", "0-100", "0.1", 0, 1, 3, dict(Coef_a=1, Coef_b=0.1, Coef_c=62, MaxRadom=4, MinRadom=0, MaxValue=100, MinValue=0)),
    ]),
    (12, 12, "[S]流量-FM200", "过程", [
        ("瞬时流量", "m3/h", "0-200", "0.01", 0, 1, 3, dict(Coef_a=1, Coef_b=0.01, Coef_c=125, MaxRadom=8, MinRadom=0, MaxValue=200, MinValue=0)),
        ("累计流量", "m3", "0-999999", "0.1", 0, 1, 0, dict(Coef_c=12580, MaxRadom=12580, MinRadom=12580, MaxValue=999999, MinValue=0)),
    ]),
    (13, 13, "[S]压力-PT300", "过程", [
        ("压力", "MPa", "0-1.6", "0.001", 0, 1, 3, dict(Coef_a=1, Coef_b=0.001, Coef_c=85, MaxRadom=5, MinRadom=0, MaxValue=1600, MinValue=0)),
    ]),
    (14, 14, "[S]电表-DTS666", "能源", [
        ("有功功率", "kW", "0-500", "0.01", 0, 1, 3, dict(Coef_a=2, Coef_b=0.01, Coef_c=128, MaxRadom=15, MinRadom=0, MaxValue=500, MinValue=0)),
        ("正向电能", "kWh", "0-999999", "0.01", 0, 1, 0, dict(Coef_c=45678, MaxRadom=45678, MinRadom=45678, MaxValue=999999, MinValue=0)),
    ]),
    (15, 15, "[S]UPS-UP500", "动力", [
        ("负载率", "%", "0-100", "0.1", 0, 1, 3, dict(Coef_a=1, Coef_b=0.1, Coef_c=42, MaxRadom=5, MinRadom=0, MaxValue=100, MinValue=0)),
        ("电池SOC", "%", "0-100", "1", 2, 1, 3, dict(Coef_a=0, Coef_b=1, Coef_c=88, MaxRadom=2, MinRadom=0, MaxValue=100, MinValue=0)),
    ]),
    (16, 16, "[S]门禁-AC100", "安防", [
        ("门状态", "", "0-65535", "1", 7, 1, 0, dict(Coef_c=1, MaxRadom=1, MinRadom=1, MaxValue=255, MinValue=1), [("1", "关闭"), ("2", "开启"), ("255", "报警")]),
        ("今日刷卡", "次", "0-9999", "1", 2, 1, 0, dict(Coef_c=37, MaxRadom=37, MinRadom=37, MaxValue=9999, MinValue=0)),
    ]),
    (17, 17, "[S]人体-PIR200", "安防", [
        ("占用状态", "", "0-65535", "1", 7, 1, 0, dict(Coef_c=1, MaxRadom=1, MinRadom=1, MaxValue=255, MinValue=1), [("1", "无人"), ("2", "有人")]),
    ]),
    (18, 18, "[S]振动-VS100", "机械", [
        ("振动速度", "mm/s", "0-50", "0.01", 0, 1, 3, dict(Coef_a=1, Coef_b=0.01, Coef_c=12, MaxRadom=2, MinRadom=0, MaxValue=50, MinValue=0)),
        ("轴承温度", "°C", "0-120", "0.1", 0, 0, 3, dict(Coef_a=1, Coef_b=0.1, Coef_c=45, MaxRadom=3, MinRadom=0, MaxValue=120, MinValue=0)),
    ]),
    (19, 19, "[S]SF6-SF600", "电力", [
        ("SF6浓度", "ppm", "0-3000", "1", 2, 1, 3, dict(Coef_a=0, Coef_b=1, Coef_c=120, MaxRadom=10, MinRadom=0, MaxValue=3000, MinValue=0)),
    ]),
    (20, 20, "[S]臭氧-O3M100", "环境", [
        ("臭氧", "ppb", "0-500", "1", 2, 1, 3, dict(Coef_a=1, Coef_b=1, Coef_c=28, MaxRadom=5, MinRadom=0, MaxValue=500, MinValue=0)),
    ]),
    (21, 21, "[S]甲醛-HCHO50", "环境", [
        ("甲醛", "mg/m3", "0-5", "0.001", 0, 1, 3, dict(Coef_a=1, Coef_b=0.001, Coef_c=8, MaxRadom=1, MinRadom=0, MaxValue=5000, MinValue=0)),
    ]),
    (22, 22, "[S]TVOC-VOC300", "环境", [
        ("TVOC", "ppb", "0-2000", "1", 2, 1, 3, dict(Coef_a=1, Coef_b=1, Coef_c=220, MaxRadom=30, MinRadom=0, MaxValue=2000, MinValue=0)),
    ]),
    (23, 23, "[S]继电器-RLY8", "控制", [
        ("输出字", "", "0-65535", "1", 7, 1, 0, dict(Coef_c=170, MaxRadom=170, MinRadom=170, MaxValue=65535, MinValue=0)),
    ]),
    (24, 24, "[S]DI采集-IO16", "控制", [
        ("DI状态", "", "0-65535", "1", 7, 1, 0, dict(Coef_c=4369, MaxRadom=4369, MinRadom=4369, MaxValue=65535, MinValue=0)),
    ]),
    (25, 25, "[S]AI采集-AI8", "控制", [
        ("AI通道1", "mA", "4-20", "0.001", 0, 1, 3, dict(Coef_a=0, Coef_b=0.001, Coef_c=12000, MaxRadom=200, MinRadom=0, MaxValue=20000, MinValue=4000)),
        ("AI通道2", "mA", "4-20", "0.001", 0, 1, 3, dict(Coef_a=0, Coef_b=0.001, Coef_c=8500, MaxRadom=150, MinRadom=0, MaxValue=20000, MinValue=4000)),
    ]),
    (26, 26, "[S]变频器-VFD750", "驱动", [
        ("输出频率", "Hz", "0-50", "0.01", 0, 1, 3, dict(Coef_a=1, Coef_b=0.01, Coef_c=3500, MaxRadom=50, MinRadom=0, MaxValue=5000, MinValue=0)),
        ("输出电流", "A", "0-100", "0.01", 0, 1, 3, dict(Coef_a=1, Coef_b=0.01, Coef_c=1850, MaxRadom=20, MinRadom=0, MaxValue=10000, MinValue=0)),
    ]),
    (27, 27, "[S]软启动-SS100", "驱动", [
        ("运行状态", "", "0-65535", "1", 7, 1, 0, dict(Coef_c=2, MaxRadom=2, MinRadom=2, MaxValue=255, MinValue=1), [("1", "停止"), ("2", "运行"), ("3", "故障")]),
        ("电机电流", "A", "0-200", "0.1", 0, 1, 3, dict(Coef_a=1, Coef_b=0.1, Coef_c=65, MaxRadom=5, MinRadom=0, MaxValue=200, MinValue=0)),
    ]),
    (28, 28, "[S]空调-ACU200", "暖通", [
        ("回风温度", "°C", "10-40", "0.1", 0, 0, 3, dict(Coef_a=1, Coef_b=0.1, Coef_c=245, MaxRadom=2, MinRadom=0, MaxValue=400, MinValue=100)),
        ("设定温度", "°C", "16-30", "0.1", 0, 0, 0, dict(Coef_c=260, MaxRadom=260, MinRadom=260, MaxValue=300, MinValue=160)),
    ]),
    (29, 29, "[S]柴发-DG500", "动力", [
        ("转速", "rpm", "0-1800", "1", 2, 1, 3, dict(Coef_a=2, Coef_b=1, Coef_c=1500, MaxRadom=20, MinRadom=0, MaxValue=1800, MinValue=0)),
        ("油位", "%", "0-100", "1", 2, 1, 3, dict(Coef_a=0, Coef_b=1, Coef_c=72, MaxRadom=2, MinRadom=0, MaxValue=100, MinValue=0)),
    ]),
    (30, 30, "[S]BMS-BMS48", "储能", [
        ("SOC", "%", "0-100", "0.1", 0, 1, 3, dict(Coef_a=0, Coef_b=0.1, Coef_c=765, MaxRadom=3, MinRadom=0, MaxValue=1000, MinValue=0)),
        ("总电压", "V", "40-60", "0.01", 0, 1, 3, dict(Coef_a=0, Coef_b=0.01, Coef_c=5120, MaxRadom=10, MinRadom=0, MaxValue=6000, MinValue=4000)),
    ]),
    (31, 31, "[S]逆变器-PV1000", "能源", [
        ("发电功率", "kW", "0-1000", "0.1", 0, 1, 3, dict(Coef_a=2, Coef_b=0.1, Coef_c=420, MaxRadom=30, MinRadom=0, MaxValue=10000, MinValue=0)),
        ("日发电量", "kWh", "0-9999", "0.1", 0, 1, 0, dict(Coef_c=2856, MaxRadom=2856, MinRadom=2856, MaxValue=99990, MinValue=0)),
    ]),
    (32, 32, "[S]变压器-TT200", "电力", [
        ("油温", "°C", "0-120", "0.1", 0, 0, 3, dict(Coef_a=1, Coef_b=0.1, Coef_c=55, MaxRadom=2, MinRadom=0, MaxValue=1200, MinValue=0)),
        ("绕组温度", "°C", "0-150", "0.1", 0, 0, 3, dict(Coef_a=1, Coef_b=0.1, Coef_c=68, MaxRadom=3, MinRadom=0, MaxValue=1500, MinValue=0)),
    ]),
]


def parse_project(path: Path) -> ET.ElementTree:
    raw = path.read_bytes()
    for encoding in ("utf-8-sig", "gb18030", "gbk"):
        try:
            text = raw.decode(encoding)
            return ET.ElementTree(ET.fromstring(text))
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


def add_data(parent: ET.Element, data_id: int, reg_addr: int, name: str, unit: str,
             range_text: str, gain: str, show_type: int, prtc_type: int, color: str,
             group: str, strategy: int, mock: dict, enums: list[tuple[str, str]] | None = None) -> None:
    data = ET.SubElement(parent, "DATA", {
        "ID": str(data_id), "Name": name, "BLOCK": "2", "Addr": str(reg_addr),
        "PrtcTYPE": str(prtc_type), "PointNum": "1" if show_type == 0 else "0",
        "Unit": unit, "ShowType": str(show_type), "Gain": gain, "Offset": "0",
        "sz": "2", "BitOffset": "0", "BitNum": "16", "Range": range_text,
        "CmdValue": "", "Color": color, "ByteOder": "0", "WordOder": "1",
        "IntervalTime": "1000", "IsBRead": "1",
    })
    ET.SubElement(data, "TIMEOUT", {"Time": "2000", "ResendCount": "0"})
    value_attrs = {
        "Stategy": str(strategy), "TimeBngrWay": "1" if strategy == 3 else "0",
        "Coef_a": str(mock.get("Coef_a", 0)), "Coef_b": str(mock.get("Coef_b", 0)),
        "Coef_c": str(mock.get("Coef_c", 0)), "Coef_d": "0", "Coef_e": "0",
        "ResetStep": "12" if strategy == 3 else "0", "XInvl": "1000",
        "MaxRadom": str(mock.get("MaxRadom", mock.get("MaxValue", 100))),
        "MinRadom": str(mock.get("MinRadom", mock.get("MinValue", 0))),
        "MaxValue": str(mock.get("MaxValue", 100)), "MinValue": str(mock.get("MinValue", 0)),
        "datarry": "", "BindDeviceID": "0", "BindDataID": "0",
    }
    ET.SubElement(data, "VALUE", value_attrs)
    enums_el = ET.SubElement(data, "ENUMS")
    for val, ename in enums or []:
        ET.SubElement(enums_el, "enum", {"Value": val, "Name": ename})
    groups = ET.SubElement(data, "GROUPS")
    ET.SubElement(groups, "group", {"Name": group})


def add_device(device_list: ET.Element, dev_id: int, addr: int, name: str, group: str, points: list) -> None:
    device = ET.SubElement(device_list, "DEVICE", {
        "ID": str(dev_id), "Name": name, "DeviceType": "2", "BindMode": "0",
        "BindDeviceID": "0", "p_invl": "1", "btime": "0", "BathReadMode": "0",
        "cGp": group, "Addr": str(addr), "mr_reg": "125", "mr_bit": "2000",
        "AddrUIMode": "0", "CRCByteOder": "1", "BlockBitOder": "0", "AddrOffset": "0",
        "PackWay": "1", "BitGap": "0", "RegGap": "5", "OneReg10": "0", "OneCoil0F": "0",
        "BCast1": "0", "BCast2": "0", "IsBCDevice": "0",
    })
    ports = ET.SubElement(device, "PORTS")
    ET.SubElement(ports, "port", {"Name": PORT})
    groups = ET.SubElement(device, "GROUPS")
    ET.SubElement(groups, "group", {"Name": group})
    ET.SubElement(device, "SELF_LIST")
    data_list = ET.SubElement(device, "DATA_LIST")
    for idx, pt in enumerate(points, start=1):
        pname, unit, rng, gain, show_type, prtc_type, strategy, mock = pt[:8]
        enums = pt[8] if len(pt) > 8 else None
        color = COLORS[(dev_id + idx) % len(COLORS)]
        add_data(data_list, idx, idx - 1, pname, unit, rng, gain, show_type, prtc_type, color, group, strategy, mock, enums)


def update_header(sys_data: ET.Element) -> None:
    for chipv in sys_data.findall("CHIPV"):
        if chipv.get("ID") == "1":
            chipv.set("ExtroPara",
                      "text::VelaGuard vgrs485 Mock 总线（9600 N81 · 从站1-32）;"
                      "fsize::22;t_bd::ON;color::#FF111827;backcolor::#00000000;")


def add_overview_page(root: ET.Element) -> None:
    sys_data = root.find("SYS_DATA")
    device_list = root.find("DEVICE_LIST")
    if sys_data is None or device_list is None:
        return

    # page tabs
    max_wid = max(int(c.get("ID", "0")) for c in sys_data.findall("CHIPV"))
    tab_id = max_wid + 1
    tab_def = ET.SubElement(sys_data, "CHIPV", {
        "ID": str(tab_id), "lock": "0", "Type": "28", "Intvl": "10",
        "ExtroPara": "text::首页;pageid::1;fsize::14;color::#FF111827;backcolor::#FFFFFFFF;",
    })
    tab_def2 = ET.SubElement(sys_data, "CHIPV", {
        "ID": str(tab_id + 1), "lock": "0", "Type": "28", "Intvl": "10",
        "ExtroPara": "text::设备总览;pageid::2;fsize::14;color::#FF111827;backcolor::#FFFFFFFF;",
    })

    page1 = sys_data.find("PAGE[@ID='1']")
    if page1 is not None:
        ET.SubElement(page1, "CHIPV", {"ID": str(tab_id), "Width": "120", "Height": "36", "PosX": "800", "PosY": "20"})
        ET.SubElement(page1, "CHIPV", {"ID": str(tab_id + 1), "Width": "120", "Height": "36", "PosX": "930", "PosY": "20"})

    page2 = ET.SubElement(sys_data, "PAGE", {
        "ID": "2", "Name": "OVERVIEW", "width": "1920", "hight": "920",
        "BackColor": "#ffffffff", "PicPath": "",
    })
    ET.SubElement(page2, "CHIPV", {"ID": str(tab_id), "Width": "120", "Height": "36", "PosX": "800", "PosY": "20"})
    ET.SubElement(page2, "CHIPV", {"ID": str(tab_id + 1), "Width": "120", "Height": "36", "PosX": "930", "PosY": "20"})

    title_id = tab_id + 2
    ET.SubElement(page2, "CHIPV", {"ID": str(title_id), "Width": "760", "Height": "50", "PosX": "20", "PosY": "20"})
    ET.SubElement(sys_data, "CHIPV", {
        "ID": str(title_id), "lock": "0", "Type": "18", "Intvl": "10",
        "ExtroPara": "text::VelaGuard RS485 从站设备总览（32 类 · Addr 1-32）;"
                     "fsize::22;t_bd::ON;color::#FF111827;backcolor::#00000000;",
    })

    tbl_id = title_id + 1
    tbl = ET.SubElement(sys_data, "CHIPV", {
        "ID": str(tbl_id), "lock": "0", "Type": "0", "Intvl": "10",
        "ExtroPara": "vcolmnHead::ON;uint::ON;vrange::ON;tlcolor::#FF111827;"
                     "vlcolor::#FF111827;gdcolor::#FFE5E7EB;backcolor::#FFFFFFFF;",
    })
    ET.SubElement(page2, "CHIPV", {"ID": str(tbl_id), "Width": "1880", "Height": "820", "PosX": "20", "PosY": "80"})

    for device in sorted(device_list.findall("DEVICE"), key=lambda d: int(d.get("ID", "0"))):
        dev_id = int(device.get("ID", "0"))
        data = device.find("DATA_LIST/DATA")
        if data is None:
            continue
        data_id = data.get("ID", "1")
        ET.SubElement(tbl, "DeviceData", {"DeviceID": str(dev_id), "DataID": data_id, "ExtroPara": ""})


def extend_hisdata(root: ET.Element) -> None:
    his = root.find("HISDATA")
    if his is None:
        return
    existing = {int(d.get("ID", "0")) for d in his.findall("DEVICE")}
    for dev_id, _, _, _, points in NEW_DEVICES:
        if dev_id in existing:
            continue
        dev = ET.SubElement(his, "DEVICE", {"ID": str(dev_id)})
        for idx in range(1, min(len(points), 2) + 1):
            ET.SubElement(dev, "DATA", {"ID": str(idx)})


def main() -> int:
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else TARGET
    tree = parse_project(path)
    root = tree.getroot()
    device_list = root.find("DEVICE_LIST")
    if device_list is None:
        raise SystemExit("missing DEVICE_LIST")

    existing_ids = {int(d.get("ID", "0")) for d in device_list.findall("DEVICE")}
    for spec in NEW_DEVICES:
        dev_id = spec[0]
        if dev_id in existing_ids:
            continue
        add_device(device_list, *spec)

    sys_data = root.find("SYS_DATA")
    if sys_data is not None:
        update_header(sys_data)
        if sys_data.find("PAGE[@ID='2']") is None:
            add_overview_page(root)
    extend_hisdata(root)

    indent(root)
    tree.write(path, encoding="UTF-8", xml_declaration=True)
    print(f"Updated {path}: {len(device_list.findall('DEVICE'))} devices")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
