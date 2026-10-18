#!/usr/bin/env python3
"""生成阶段 F 阈值告警验收用的工况数据。

输入是已确认点表的一份 JSON 副本（默认 scripts/vgpoint_scene_room.json）。
输出两份同源产物：

  scenarios.json                 板验操作清单的数据来源（工况、控制行、预期、原始值）
  alarm_scenarios_fixture.h      离线测试用的 C 数据表（点表 + 各步骤的原始值/预期）

预期结果按 docs/velaguard-host-nsh-protocol.md 第 4 节的告警规则独立计算，
不复用固件实现，避免用同一份实现的结果自我验证。数值按 IEEE 单精度
（float32）推算，与板端 `(float)raw * scale` 的运算宽度一致。

用法：
  python3 scripts/alarm_scenarios/gen_alarm_scenarios.py            # 重新生成
  python3 scripts/alarm_scenarios/gen_alarm_scenarios.py --check    # 校验已提交产物是否最新
  python3 scripts/alarm_scenarios/gen_alarm_scenarios.py --list-dump <file>
                                                                    # 用板端 vgpoint list 输出核对点表
"""

import argparse
import json
import struct
import sys
from pathlib import Path

# 与 app/velaguard/vg_alarm_eval.c 的 VG_ALARM_EQ_EPS 一致
EQ_EPS = struct.unpack("<f", struct.pack("<f", 0.0005))[0]

# 与 app/velaguard/vg_alarm_eval.c 的滑窗一致
WINDOW_SAMPLES = 8
WINDOW_MIN_FAILURES = 5

KIND_NONE = 0
KIND_WARN = 1
KIND_CRIT = 2
KIND_OFFLINE = 3

KIND_NAME = {
    KIND_NONE: "none",
    KIND_WARN: "warn",
    KIND_CRIT: "crit",
    KIND_OFFLINE: "offline",
}

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_TABLE = REPO_ROOT / "scripts/vgpoint_scene_room.json"
DEFAULT_JSON = REPO_ROOT / "scripts/alarm_scenarios/scenarios.json"
DEFAULT_HEADER = REPO_ROOT / "scripts/alarm_scenarios/alarm_scenarios_fixture.h"


def f32(value):
    """按 IEEE 单精度舍入，复现板端 float 运算结果。"""
    return struct.unpack("<f", struct.pack("<f", value))[0]


def raw_bounds(dtype):
    if dtype == "int16":
        return -32768, 32767
    return 0, 65535


def decode_raw(raw, dtype, scale):
    """复现 vg_ui_backend_board.c 的换算：先按 dtype 取有符号/无符号，再乘 scale。"""
    value = raw - 0x10000 if (dtype == "int16" and raw >= 0x8000) else raw
    return f32(f32(float(value)) * f32(scale))


def rule_kind(point, value):
    """按协议第 4 节的规则判定告警种类。"""
    cmp_ = point.get("cmp") or ""
    if cmp_ == "":
        return KIND_NONE

    has_warn = "warn" in point
    has_crit = "crit" in point
    warn = f32(point["warn"]) if has_warn else None
    crit = f32(point["crit"]) if has_crit else None

    if cmp_ == "eq":
        if has_crit and abs(value - crit) <= EQ_EPS:
            return KIND_CRIT
        if has_warn and abs(value - warn) <= EQ_EPS:
            return KIND_WARN
        return KIND_NONE

    if cmp_ == "ge":
        if has_crit and value >= crit:
            return KIND_CRIT
        if has_warn and value >= warn:
            return KIND_WARN
        return KIND_NONE

    if cmp_ == "le":
        if has_crit and value <= crit:
            return KIND_CRIT
        if has_warn and value <= warn:
            return KIND_WARN
        return KIND_NONE

    return KIND_NONE


def boundary_raws(point, target):
    """给出使解码值等于/小于/大于 target 的原始值。

    返回 (exact, below, above)。exact 为 None 表示该阈值在 scale 的网格上
    无法精确命中。below/above 为最接近的两侧取值。
    """
    lo, hi = raw_bounds(point["dtype"])
    scale = f32(point["scale"])
    guess = int(round(f32(target) / scale))
    exact = None
    below = None
    above = None

    for raw in range(guess - 8, guess + 9):
        if raw < lo or raw > hi:
            continue
        value = decode_raw(raw, point["dtype"], scale)
        if value == f32(target):
            exact = raw
        elif value < f32(target):
            if below is None or raw > below:
                below = raw
        elif above is None or raw < above:
            above = raw

    return exact, below, above


def point_raw_spec(point, raw):
    value = decode_raw(raw, point["dtype"], point["scale"])
    return {
        "point": point["id"],
        "raw": raw,
        "value": value,
        "expect": KIND_NAME[rule_kind(point, value)],
    }


def describe(value, threshold, relation):
    return "%s %s %s" % (fmt(value), relation, fmt(threshold))


def fmt(value):
    text = "%.6g" % value
    return text


def threshold_steps(point, threshold_name, threshold):
    """ge/le 的一个阈值给出三步：阈值下方、等于阈值、阈值上方。"""
    exact, below, above = boundary_raws(point, threshold)
    steps = []

    if point["cmp"] == "ge":
        pairs = [
            ("低于 %s" % threshold_name, below),
            ("等于 %s" % threshold_name, exact),
            ("高于 %s" % threshold_name, above),
        ]
    else:
        pairs = [
            ("高于 %s" % threshold_name, above),
            ("等于 %s" % threshold_name, exact),
            ("低于 %s" % threshold_name, below),
        ]

    for label, raw in pairs:
        if raw is None:
            continue
        spec = point_raw_spec(point, raw)
        spec["label"] = "%s（%s）" % (label, fmt(spec["value"]))
        spec["control"] = "value %d %d %d" % (point["addr"], point["reg"], raw)
        spec["exact"] = (raw == exact)
        steps.append(spec)

    return steps


def eq_steps(point, threshold_name, threshold):
    exact, below, above = boundary_raws(point, threshold)
    steps = []
    for label, raw in (("低于 %s" % threshold_name, below),
                       ("等于 %s" % threshold_name, exact),
                       ("高于 %s" % threshold_name, above)):
        if raw is None:
            continue
        spec = point_raw_spec(point, raw)
        spec["label"] = "%s（%s）" % (label, fmt(spec["value"]))
        spec["control"] = "value %d %d %d" % (point["addr"], point["reg"], raw)
        spec["exact"] = (raw == exact)
        steps.append(spec)
    return steps


def load_table(path):
    with open(path, "r", encoding="utf-8") as handle:
        doc = json.load(handle)
    if doc.get("schema_version") != 1:
        raise SystemExit("unsupported schema_version in %s" % path)
    points = doc.get("points") or []
    if not points:
        raise SystemExit("no points in %s" % path)
    seen = set()
    for point in points:
        if point["id"] in seen:
            raise SystemExit("duplicate id %s in %s" % (point["id"], path))
        seen.add(point["id"])
        if point["dtype"] not in ("int16", "uint16"):
            raise SystemExit("unsupported dtype %s" % point["dtype"])
        if not point.get("fc"):
            point["fc"] = 3
        if point.get("cmp") and (("warn" in point) != ("crit" in point)):
            pass  # 单阈值合法
        if point.get("cmp") == "ge" and "warn" in point and "crit" in point:
            if point["warn"] > point["crit"]:
                raise SystemExit("ge with warn > crit: %s" % point["id"])
        if point.get("cmp") == "le" and "warn" in point and "crit" in point:
            if point["warn"] < point["crit"]:
                raise SystemExit("le with warn < crit: %s" % point["id"])
    return doc, points


def baseline_raws(points):
    """与 scripts/maintenance_rtu_slave.ps1 默认 normal 模式一致的基线原始值。"""
    table = {
        (1, 0): 240,
        (1, 1): 500,
        (2, 2): 0,
        (3, 0): 10,
        (4, 0): 250,
        (4, 1): 240,
        (4, 2): 1,
        (5, 0): 400,
        (5, 1): 80,
        (5, 2): 0,
        (6, 0): 2300,
        (6, 1): 300,
        (6, 2): 1234,
        (7, 0): 0,
    }
    raws = {}
    for point in points:
        key = (point["addr"], point["reg"])
        if key not in table:
            raise SystemExit(
                "point %s at addr=%d reg=%d has no baseline value in "
                "maintenance_rtu_slave.ps1; add it before running the "
                "scenarios" % (point["id"], point["addr"], point["reg"])
            )
        raws[point["id"]] = table[key]
    return raws


def c_float(value):
    """C 里的单精度字面量：整数也要带小数点。"""
    text = "%.9g" % value
    if "." not in text and "e" not in text and "E" not in text and "n" not in text:
        text += ".0"
    return text + "f"


def offline_spec(point, raw, fails, online_after=None):
    """一个只喂失败采样的步骤项；raw 是该点本应读到的值，供对照。"""
    entry = {
        "point": point["id"],
        "raw": raw,
        "value": decode_raw(raw, point["dtype"], point["scale"]),
        "offline_n": fails,
        "expect": "offline" if fails >= WINDOW_MIN_FAILURES else "none",
    }
    if online_after is not None:
        entry["online_after"] = online_after
    return entry


def mk_step(label, control, expects, observe, active=None, primary=None):
    """构造一个工况步骤。active/primary 不给时按本步的 expects 推导。"""
    entry = {
        "label": label,
        "control": control,
        "expects": expects,
        "observe": observe,
    }
    if active is None:
        active = len([e for e in expects if e["expect"] != "none"])
    entry["expect_active"] = active
    if primary is not None:
        entry["expect_primary"] = {"point": primary[0], "kind": primary[1]}
    return entry


def build_cases(points):
    by_id = {point["id"]: point for point in points}
    base = baseline_raws(points)
    cases = []

    # B1 基线
    cases.append({
        "id": "B1-baseline",
        "title": "正常读数基线",
        "steps": [
            mk_step("全部从站正常读数", "reset",
                    [point_raw_spec(by_id[pid], raw) for pid, raw in base.items()],
                    "14 点全部在线，首页无告警 chip，无 pending_alarm.txt",
                    active=0, primary=("", "none")),
        ],
    })

    # B2 传统三档模式（脚本既有模式，作为默认行为兼容回归）
    cases.append({
        "id": "B2-legacy-modes",
        "title": "脚本既有 warn / critical / normal 模式（默认行为兼容）",
        "steps": [
            mk_step("warn 模式", "warn", [point_raw_spec(by_id["temp"], 300)],
                    "温度 30.0 C >= warn 28，判预警", active=1,
                    primary=("temp", "warn")),
            mk_step("critical 模式", "critical",
                    [point_raw_spec(by_id["temp"], 360)],
                    "温度 36.0 C >= crit 35，判严重", active=1,
                    primary=("temp", "crit")),
            mk_step("normal 模式", "normal", [point_raw_spec(by_id["temp"], 240)],
                    "温度 24.0 C，恢复", active=0, primary=("", "none")),
        ],
    })

    # B3 逐点阈值边界
    for point in points:
        cmp_ = point.get("cmp") or ""
        if cmp_ == "":
            continue
        specs = []
        if cmp_ in ("ge", "le"):
            if "warn" in point:
                specs += threshold_steps(point, "warn", point["warn"])
            if "crit" in point:
                specs += threshold_steps(point, "crit", point["crit"])
        else:
            if "crit" in point:
                specs += eq_steps(point, "crit", point["crit"])
            if "warn" in point:
                specs += eq_steps(point, "warn", point["warn"])
        steps = []
        for spec in specs:
            primary = ("", "none") if spec["expect"] == "none" else (point["id"], spec["expect"])
            steps.append(mk_step(spec["label"], spec["control"], [spec],
                                 "期望 %s" % spec["expect"], primary=primary))
        cases.append({
            "id": "B3-%s-%s" % (cmp_, point["id"]),
            "title": "%s 边界：%s（%s）" % (cmp_, point["name"], point["id"]),
            "steps": steps,
        })

    # B4 int16 有符号解码（负数原始值）
    acu_ret = by_id["acu_ret"]
    raw_neg = 0xFF9C
    cases.append({
        "id": "B4-int16-negative",
        "title": "int16 负数解码（回风温度）",
        "steps": [
            mk_step("原始值 0xFF9C 应按 int16 解为 -10.0 C",
                    "value %d %d %d" % (acu_ret["addr"], acu_ret["reg"], raw_neg),
                    [point_raw_spec(acu_ret, raw_neg)],
                    "屏上回风温度为负值且不告警；若按 uint16 解码会得到 6543.6 C 并误报严重",
                    primary=("", "none")),
        ],
    })

    # B5 cmp 为空的点不做模拟量告警
    no_cmp = [point for point in points if not (point.get("cmp") or "")]
    steps = []
    for point in no_cmp:
        raw = 0xFFFF if point["dtype"] == "uint16" else 30000
        steps.append(mk_step("%s 取极端值仍不告警" % point["name"],
                             "value %d %d %d" % (point["addr"], point["reg"], raw),
                             [point_raw_spec(point, raw)],
                             "%s 无 cmp，只采集" % point["id"],
                             primary=("", "none")))
    cases.append({
        "id": "B5-no-cmp",
        "title": "cmp 为空的点只采集、不产生模拟量告警",
        "steps": steps,
    })

    # I1 单点 warn 升级到 crit，同一次告警期
    cases.append({
        "id": "I1-warn-to-crit",
        "title": "同点 warn 升级为 crit：告警期连续，不新增事件",
        "steps": [
            mk_step("温度到 warn", "value 1 0 280", [point_raw_spec(by_id["temp"], 280)],
                    "事件日志新增 1 条 raise threshold_high", active=1,
                    primary=("temp", "warn")),
            mk_step("温度升到 crit", "value 1 0 350", [point_raw_spec(by_id["temp"], 350)],
                    "告警期延续，al_epoch 不变；事件日志不新增 raise；屏上等级变为严重",
                    active=1, primary=("temp", "crit")),
        ],
    })

    # I2 恢复：告警期结束
    cases.append({
        "id": "I2-recover",
        "title": "阈值恢复：告警期结束",
        "steps": [
            mk_step("温度到 crit", "value 1 0 350", [point_raw_spec(by_id["temp"], 350)],
                    "严重告警", active=1, primary=("temp", "crit")),
            mk_step("温度回正常", "value 1 0 240", [point_raw_spec(by_id["temp"], 240)],
                    "告警页收起，pending_alarm.txt 被删除，事件日志新增 clear",
                    active=0, primary=("", "none")),
        ],
    })

    # I3 恢复后再次触发
    cases.append({
        "id": "I3-retrigger",
        "title": "恢复后再次触发：不漏报、不重复",
        "steps": [
            mk_step("第一次 warn", "value 1 0 280", [point_raw_spec(by_id["temp"], 280)],
                    "第 1 次 raise 事件", active=1, primary=("temp", "warn")),
            mk_step("保持 warn 再读若干轮", "",
                    [point_raw_spec(by_id["temp"], 280)],
                    "不重复产生 raise 事件，al_epoch 不变", active=1,
                    primary=("temp", "warn")),
            mk_step("回正常", "value 1 0 240", [point_raw_spec(by_id["temp"], 240)],
                    "clear 事件，pending_alarm.txt 删除", active=0,
                    primary=("", "none")),
            mk_step("第二次 warn", "value 1 0 280", [point_raw_spec(by_id["temp"], 280)],
                    "第 2 次 raise，pending_alarm.txt 重新出现", active=1,
                    primary=("temp", "warn")),
        ],
    })

    # I4 多点优先级：严重压过预警，且单点恢复不动邻点告警期
    cases.append({
        "id": "I4-primary-crit-over-warn",
        "title": "同时告警的优先级：严重压过预警",
        "steps": [
            mk_step("温度预警 + 水浸严重", "value 1 0 280\nvalue 2 2 1",
                    [point_raw_spec(by_id["temp"], 280),
                     point_raw_spec(by_id["flood"], 1)],
                    "主告警为水浸（严重）；pending tag=flood/threshold", active=2,
                    primary=("flood", "crit")),
            mk_step("水浸恢复，温度仍在预警", "value 2 2 0",
                    [point_raw_spec(by_id["flood"], 0)],
                    "水浸告警期结束，温度告警期保留；主告警切换为温度，"
                    "pending_alarm.txt 重写为 temp/threshold", active=1,
                    primary=("temp", "warn")),
        ],
    })

    # I5 多点优先级：严重压过离线
    cases.append({
        "id": "I5-primary-crit-over-offline",
        "title": "同时告警的优先级：严重压过离线",
        "steps": [
            mk_step("温度严重 + 5 号从站静默", "value 1 0 350\noffline 5",
                    [point_raw_spec(by_id["temp"], 350),
                     offline_spec(by_id["ups_load"], base["ups_load"], 6),
                     offline_spec(by_id["ups_soc"], base["ups_soc"], 6),
                     offline_spec(by_id["ups_bypass"], base["ups_bypass"], 6)],
                    "静默满 5 个失败采样后 5 号从站 3 点记离线；主告警仍是温度",
                    active=4, primary=("temp", "crit")),
        ],
    })

    # I6 单从站静默，其他从站正常
    cases.append({
        "id": "I6-single-slave-silent",
        "title": "单从站静默，其他从站正常",
        "steps": [
            mk_step("5 号从站静默", "offline 5",
                    [offline_spec(by_id["ups_load"], base["ups_load"], 6),
                     offline_spec(by_id["ups_soc"], base["ups_soc"], 6),
                     offline_spec(by_id["ups_bypass"], base["ups_bypass"], 6)],
                    "只有 5 号从站的 3 个点离线，其余 11 点保持正常；"
                    "主告警按点表顺序取 ups_load", active=3,
                    primary=("ups_load", "offline")),
            mk_step("5 号从站恢复", "normal",
                    [point_raw_spec(by_id["ups_load"], base["ups_load"]),
                     point_raw_spec(by_id["ups_soc"], base["ups_soc"]),
                     point_raw_spec(by_id["ups_bypass"], base["ups_bypass"])],
                    "一次成功采样即恢复在线，3 个告警期同时结束，"
                    "pending_alarm.txt 删除", active=0, primary=("", "none")),
        ],
    })

    # I7 全部静默后恢复
    cases.append({
        "id": "I7-all-slaves-silent",
        "title": "全部从站静默，再恢复正常",
        "steps": [
            mk_step("全部静默", "offline",
                    [offline_spec(point, base[point["id"]], 6)
                     for point in points],
                    "14 点全部离线；主告警按点表顺序取 temp", active=len(points),
                    primary=("temp", "offline")),
            mk_step("全部恢复", "normal",
                    [point_raw_spec(point, base[point["id"]]) for point in points],
                    "同一轮全部恢复，pending_alarm.txt 删除", active=0,
                    primary=("", "none")),
        ],
    })

    # I8 离线滑窗：单点连续失败计数
    door = by_id["door"]
    window_steps = []
    for label, fails, active, primary in (
            ("本步新增 4 次失败（累计 4 次）", 4, 0, ("", "none")),
            ("本步新增 12 次失败（累计第 5 次触发）",
             12, 1, ("door", "offline"))):
        window_steps.append(mk_step(
            "%s（door，单点从站）" % label,
            "drop %d %d %d" % (door["addr"], door["reg"], fails * 2),
            [offline_spec(door, base["door"], fails,
                          online_after=0 if fails >= WINDOW_MIN_FAILURES else 1)],
            "每轮 2 次请求（1 次重试）；本步 %d 次失败，%s"
            % (fails, "前一步留下 4 个失败位，不满 5 次不触发"
               if fails < WINDOW_MIN_FAILURES
               else "本步第 1 次失败就是累计第 5 次，立即触发离线；"
                    "drop 用尽后的下一次成功采样恢复在线"),
            active=active, primary=primary))
    cases.append({
        "id": "I8-offline-window",
        "title": "离线滑窗：连续失败计数与实际窗口",
        "steps": window_steps,
    })

    # I9 离散失败：单次丢帧被重试吸收
    cases.append({
        "id": "I9-single-drop",
        "title": "离散失败：单次丢帧不产生失败采样",
        "steps": [
            mk_step("丢 1 次应答（door）",
                    "drop %d %d 1" % (door["addr"], door["reg"]),
                    [offline_spec(door, base["door"], 0, online_after=1)],
                    "板端 100 ms 后重试成功，无失败采样，无告警", active=0,
                    primary=("", "none")),
        ],
    })

    # I10 同一从站内按点独立计数
    cases.append({
        "id": "I10-per-point-window",
        "title": "同一从站内各点独立计数（ups_load 静默，邻点正常）",
        "steps": [
            mk_step("只丢 ups_load 的应答", "drop 5 0 20",
                    [offline_spec(by_id["ups_load"], base["ups_load"], 10),
                     offline_spec(by_id["ups_soc"], base["ups_soc"], 0),
                     offline_spec(by_id["ups_bypass"], base["ups_bypass"], 0)],
                    "同一从站的 ups_soc / ups_bypass 不受影响，仍在线", active=1,
                    primary=("ups_load", "offline")),
        ],
    })

    return cases


def offline_need(point):
    """Effective failed-sample threshold for one point."""
    return max(WINDOW_MIN_FAILURES,
               min(WINDOW_SAMPLES, point.get("fail_n", 3)))


def annotate_sample_history(points, cases):
    """Record where each case first enters the offline state.

    The board keeps one sliding window per point for the whole case, including
    across consecutive scenario steps.  A step's offline_n is therefore not
    enough to calculate its trigger position: failures left by earlier steps
    still occupy the window.  Simulate that documented window here so the
    fixture states the expected trigger explicitly instead of letting a
    consumer restart the count at every step.
    """
    for case in cases:
        windows = {point["id"]: [False] * WINDOW_SAMPLES for point in points}
        online = {point["id"]: True for point in points}
        by_id = {point["id"]: point for point in points}

        # Both C harnesses feed one normal baseline sample for every point
        # before running the case steps.
        for point_id in windows:
            windows[point_id].pop(0)
            windows[point_id].append(False)

        for step in case["steps"]:
            for entry in step["expects"]:
                point_id = entry["point"]
                point = by_id[point_id]
                failures = entry.get("offline_n", 0)
                window = windows[point_id]
                before = sum(window)
                trigger = 0

                if failures:
                    need = offline_need(point)
                    for sample in range(1, failures + 1):
                        was_offline = not online[point_id]
                        window.pop(0)
                        window.append(True)
                        now_offline = sum(window) >= need
                        online[point_id] = not now_offline
                        if now_offline and not was_offline and trigger == 0:
                            trigger = sample

                    entry["expect"] = ("offline" if not online[point_id]
                                        else "none")
                else:
                    window.pop(0)
                    window.append(False)
                    online[point_id] = True

                entry["offline_before"] = before
                entry["offline_after"] = sum(window)
                entry["offline_trigger"] = trigger
                entry["online_after"] = 0 if not online[point_id] else 1


def build_gaps(points):
    """记录当前点表覆盖不到的测试前置缺口。"""
    gaps = []

    eq_scaled = [p["id"] for p in points
                 if (p.get("cmp") or "") == "eq" and f32(p["scale"]) != 1.0]
    if not eq_scaled:
        gaps.append({
            "id": "G1-eq-epsilon",
            "title": "eq 判定精度（VG_ALARM_EQ_EPS = 0.0005）无法在板端覆盖",
            "detail": "点表里所有 eq 点的 scale 都是 1，阈值 1 可被整数原始值精确命中，"
                      "0.0005 的容差带取不到。需要一张 eq 且 scale<1、阈值小数不可精确"
                      "表示的候选表才能覆盖。",
            "covered_by": "app/velaguard/host_tests/test_alarm_boundary.c 的合成规则用例",
        })

    fail_n_values = sorted({p.get("fail_n", 3) for p in points})
    if fail_n_values == [3]:
        gaps.append({
            "id": "G2-fail_n",
            "title": "fail_n 的有效区间无法在板端覆盖",
            "detail": "点表里 fail_n 全为 3，有效阈值恒为 max(3,5)=5。"
                      "fail_n>5（上限 8）与 fail_n<5 被抬高的路径取不到。",
            "covered_by": "app/velaguard/host_tests/test_alarm_eval.c 与 test_alarm_boundary.c",
        })

    fc4 = [p["id"] for p in points if p.get("fc", 3) == 4]
    if not fc4:
        gaps.append({
            "id": "G3-fc04",
            "title": "FC04 输入寄存器读取路径无法在板端覆盖",
            "detail": "点表里所有点都是 fc=3。模拟从站对 FC04 有应答分支，但没有点会走这条路径。",
            "covered_by": "无；需要一张含 fc=4 点的候选表",
        })

    both_empty = [p["id"] for p in points
                  if (p.get("cmp") or "") in ("ge", "le")
                  and "warn" not in p and "crit" not in p]
    if not both_empty:
        gaps.append({
            "id": "G4-cmp-no-threshold",
            "title": "cmp 有值但 warn/crit 都空的点无法在板端覆盖",
            "detail": "协议规定这种情况不做模拟量告警，但点表里没有这样的点。",
            "covered_by": "无；需要候选表补充",
        })

    return gaps


def build_doc(points, cases, gaps, table_path):
    return {
        "schema": 1,
        "generator": "scripts/alarm_scenarios/gen_alarm_scenarios.py",
        "table_source": str(table_path.relative_to(REPO_ROOT)).replace("\\", "/"),
        "rule": {
            "source": "docs/velaguard-host-nsh-protocol.md 第 4 节",
            "eq_eps": EQ_EPS,
            "window_samples": WINDOW_SAMPLES,
            "window_min_failures": WINDOW_MIN_FAILURES,
            "rank": ["crit", "offline", "warn"],
            "recovery": "成功采样立即恢复在线；在线读数回到 none 时该点告警期结束",
        },
        "points": [
            {
                "id": point["id"],
                "name": point["name"],
                "addr": point["addr"],
                "fc": point.get("fc", 3),
                "reg": point["reg"],
                "dtype": point["dtype"],
                "scale": f32(point["scale"]),
                "unit": point.get("unit", ""),
                "cmp": point.get("cmp", ""),
                "warn": f32(point["warn"]) if "warn" in point else None,
                "crit": f32(point["crit"]) if "crit" in point else None,
                "fail_n": point.get("fail_n", 3),
            }
            for point in points
        ],
        "cases": cases,
        "gaps": gaps,
    }


def parse_list_dump(text):
    """解析板端 `vgpoint list` 输出里的 POINT 行。"""
    points = []
    for line in text.splitlines():
        if "vgpoint: POINT " not in line:
            continue
        fields = {}
        for token in line.split("vgpoint: POINT ", 1)[1].split():
            if "=" in token:
                key, value = token.split("=", 1)
                fields[key] = value
        points.append(fields)
    return points


def compare_with_dump(points, dump_text):
    dumped = parse_list_dump(dump_text)
    problems = []
    if not dumped:
        problems.append("未在输入里找到 vgpoint: POINT 行")
        return problems
    if len(dumped) != len(points):
        problems.append("点数不一致：板端 %d，点表副本 %d"
                        % (len(dumped), len(points)))

    dumped_by_id = {row.get("id"): row for row in dumped}
    for point in points:
        row = dumped_by_id.get(point["id"])
        if row is None:
            problems.append("板端缺少点 %s" % point["id"])
            continue

        def as_text(value):
            return "-" if value is None else ("%.6g" % value)

        checks = [
            ("addr", str(point["addr"])),
            ("fc", str(point.get("fc", 3))),
            ("reg", str(point["reg"])),
            ("dtype", point["dtype"]),
            ("scale", as_text(f32(point["scale"]))),
            ("unit", point.get("unit", "") or "-"),
            ("cmp", point.get("cmp", "") or "-"),
            ("warn", as_text(f32(point["warn"]) if "warn" in point else None)),
            ("crit", as_text(f32(point["crit"]) if "crit" in point else None)),
            ("fail_n", str(point.get("fail_n", 3))),
        ]
        for key, expected in checks:
            if row.get(key) != expected:
                problems.append("%s 的 %s 不一致：板端 %s，点表副本 %s"
                                % (point["id"], key, row.get(key), expected))

    for point_id in dumped_by_id:
        if point_id not in {point["id"] for point in points}:
            problems.append("板端多出点 %s" % point_id)

    return problems


KIND_NAME_REVERSE = {
    "none": KIND_NONE,
    "warn": KIND_WARN,
    "crit": KIND_CRIT,
    "offline": KIND_OFFLINE,
}


def emit_header(points, cases, path):
    lines = []
    lines.append("/* Generated by scripts/alarm_scenarios/gen_alarm_scenarios.py."
                 " Do not edit by hand. */")
    lines.append("/* Source table: scripts/vgpoint_scene_room.json */")
    lines.append("")
    lines.append("#ifndef VG_ALARM_SCENARIOS_FIXTURE_H")
    lines.append("#define VG_ALARM_SCENARIOS_FIXTURE_H")
    lines.append("")
    lines.append("struct vg_afx_point")
    lines.append("{")
    lines.append("  const char *id;")
    lines.append("  const char *name;")
    lines.append("  unsigned addr;")
    lines.append("  unsigned fc;")
    lines.append("  unsigned reg;")
    lines.append("  const char *dtype;")
    lines.append("  float scale;")
    lines.append("  const char *cmp;")
    lines.append("  const char *unit;")
    lines.append("  int has_warn;")
    lines.append("  int has_crit;")
    lines.append("  float warn;")
    lines.append("  float crit;")
    lines.append("  unsigned fail_n;")
    lines.append("};")
    lines.append("")
    lines.append("/* kind: 0 none, 1 warn, 2 crit, 3 offline (enum vg_alarm_kind order).")
    lines.append(" * raw: the 16-bit register value the simulated slave returns; for")
    lines.append(" *      int16 points a value above 0x7FFF is a negative reading.")
    lines.append(" * offline_n: >0 means feed this many failed samples instead of a read.")
    lines.append(" * offline_before/offline_after: failed bits in the eight-sample")
    lines.append(" * window before and after this step.")
    lines.append(" * offline_trigger: 1-based failed sample in this step that enters")
    lines.append(" * offline, or 0 when it does not enter during this step.")
    lines.append(" * online_after: 0/1 assert the model online flag, -1 skip. */")
    lines.append("struct vg_afx_expect")
    lines.append("{")
    lines.append("  unsigned point;")
    lines.append("  unsigned raw;")
    lines.append("  unsigned offline_n;")
    lines.append("  int kind;")
    lines.append("  int online_after;")
    lines.append("  unsigned offline_before;")
    lines.append("  unsigned offline_after;")
    lines.append("  unsigned offline_trigger;")
    lines.append("};")
    lines.append("")
    lines.append("/* primary_kind: -1 not asserted, else the expected primary alarm kind.")
    lines.append(" * primary_point: -1 when primary_kind is none, else a point index. */")
    lines.append("struct vg_afx_step")
    lines.append("{")
    lines.append("  const char *case_id;")
    lines.append("  const char *label;")
    lines.append("  const char *board_control;")
    lines.append("  unsigned first;")
    lines.append("  unsigned count;")
    lines.append("  int expect_active;")
    lines.append("  int primary_point;")
    lines.append("  int primary_kind;")
    lines.append("};")
    lines.append("")
    lines.append("#define VG_AFX_POINT_COUNT %d" % len(points))
    lines.append("")
    lines.append("static const struct vg_afx_point vg_afx_points[] =")
    lines.append("{")
    for point in points:
        lines.append(
            '  {"%s", "%s", %d, %d, %d, "%s", %s, "%s", "%s", %d, %d, %s, %s, %d},'
            % (point["id"], point["name"], point["addr"], point.get("fc", 3),
               point["reg"], point["dtype"], c_float(f32(point["scale"])),
               point.get("cmp", "") or "", point.get("unit", "") or "",
               1 if "warn" in point else 0,
               1 if "crit" in point else 0,
               c_float(f32(point["warn"])) if "warn" in point else "0.0f",
               c_float(f32(point["crit"])) if "crit" in point else "0.0f",
               point.get("fail_n", 3)))
    lines.append("};")
    lines.append("")

    index = {point["id"]: i for i, point in enumerate(points)}
    expects = []
    steps = []
    for case in cases:
        for step in case["steps"]:
            first = len(expects)
            for entry in step["expects"]:
                offline_n = entry.get("offline_n", 0)
                raw = entry["raw"]
                expects.append((index[entry["point"]], raw, offline_n,
                                KIND_NAME_REVERSE[entry["expect"]],
                                entry.get("online_after", -1),
                                entry.get("offline_before", 0),
                                entry.get("offline_after", 0),
                                entry.get("offline_trigger", 0)))
            primary = step.get("expect_primary")
            if primary is None:
                primary_point = -1
                primary_kind = -1
            elif primary["kind"] == "none":
                primary_point = -1
                primary_kind = KIND_NONE
            else:
                primary_point = index[primary["point"]]
                primary_kind = KIND_NAME_REVERSE[primary["kind"]]
            steps.append((case["id"], step["label"], step.get("control", ""),
                          first, len(expects) - first, step["expect_active"],
                          primary_point, primary_kind))

    base = baseline_raws(points)
    lines.append("/* 正常读数基线（与维护从站脚本的 normal 模式一致） */")
    lines.append("#define VG_AFX_BASELINE_COUNT %d" % len(points))
    lines.append("")
    lines.append("static const unsigned vg_afx_baseline[VG_AFX_BASELINE_COUNT] =")
    lines.append("{")
    row = "  "
    for point in points:
        row += "%d, " % base[point["id"]]
        if len(row) > 70:
            lines.append(row.rstrip())
            row = "  "
    if row.strip():
        lines.append(row.rstrip())
    lines.append("};")
    lines.append("")
    lines.append("#define VG_AFX_EXPECT_COUNT %d" % len(expects))
    lines.append("")
    lines.append("static const struct vg_afx_expect vg_afx_expects[] =")
    lines.append("{")
    for entry in expects:
        lines.append("  {%d, %d, %d, %d, %d, %d, %d, %d},"
                     % (entry[0], entry[1], entry[2], entry[3], entry[4],
                        entry[5], entry[6], entry[7]))
    lines.append("};")
    lines.append("")
    lines.append("#define VG_AFX_STEP_COUNT %d" % len(steps))
    lines.append("")
    lines.append("static const struct vg_afx_step vg_afx_steps[] =")
    lines.append("{")
    for step in steps:
        control = (step[2] or "").replace("\n", "; ")
        control = control.replace("\\", "\\\\").replace('"', '\\"')
        label = step[1].replace("\\", "\\\\").replace('"', '\\"')
        lines.append('  {"%s", "%s", "%s", %d, %d, %d, %d, %d},'
                     % (step[0], label, control, step[3], step[4], step[5],
                        step[6], step[7]))
    lines.append("};")
    lines.append("")
    lines.append("#endif /* VG_ALARM_SCENARIOS_FIXTURE_H */")
    lines.append("")

    path.write_text("\n".join(lines), encoding="utf-8")


def check_expectations(points, cases):
    """自检：工况里的预期必须和规则计算一致，等于阈值的步骤必须真的相等。"""
    problems = []
    by_id = {point["id"]: point for point in points}
    for case in cases:
        seen = []
        for step in case["steps"]:
            for entry in step["expects"]:
                point = by_id[entry["point"]]
                if "offline_n" in entry:
                    need = offline_need(point)
                    expected = ("offline"
                                if entry["offline_after"] >= need else "none")
                    if entry["expect"] != expected:
                        problems.append("%s/%s: 窗口内 %d 次失败应判 %s，写成 %s"
                                        % (case["id"], entry["point"],
                                           entry["offline_after"], expected,
                                           entry["expect"]))
                    expected_online = 0 if expected == "offline" else 1
                    if entry["online_after"] != expected_online:
                        problems.append("%s/%s: 失败 %d 次后的在线标志写成 %d"
                                        % (case["id"], entry["point"],
                                           entry["offline_n"],
                                           entry["online_after"]))
                    if entry["offline_trigger"] > entry["offline_n"]:
                        problems.append("%s/%s: 离线触发位置 %d 超出本步失败数 %d"
                                        % (case["id"], entry["point"],
                                           entry["offline_trigger"],
                                           entry["offline_n"]))
                    if (entry["offline_before"] < need
                            and entry["offline_before"]
                                + entry["offline_n"] >= need):
                        expected_trigger = need - entry["offline_before"]
                        if entry["offline_trigger"] != expected_trigger:
                            problems.append(
                                "%s/%s: 应在累计第 %d 次失败触发，工况写 %d"
                                % (case["id"], entry["point"],
                                   expected_trigger,
                                   entry["offline_trigger"]))
                else:
                    value = decode_raw(entry["raw"], point["dtype"], point["scale"])
                    if value != f32(entry["value"]):
                        problems.append(
                            "%s/%s: 原始值 %d 的解码值 %r 与记录 %r 不一致"
                            % (case["id"], entry["point"], entry["raw"],
                               value, entry["value"]))
                    kind = KIND_NAME[rule_kind(point, value)]
                    if kind != entry["expect"]:
                        problems.append("%s/%s: 规则判 %s，工况写 %s"
                                        % (case["id"], entry["point"], kind,
                                           entry["expect"]))
                    if (entry.get("exact") and kind == "none"
                            and point.get("cmp") in ("ge", "le")):
                        problems.append("%s/%s: 标记为等于阈值却判 none"
                                        % (case["id"], entry["point"]))
                seen.append((entry["point"], entry["expect"]))

            alarming = [e for e in step["expects"] if e["expect"] != "none"]
            if step["expect_active"] < len(alarming):
                problems.append("%s/%s: 活动告警数 %d 少于本步断言的 %d 条"
                                % (case["id"], step["label"],
                                   step["expect_active"], len(alarming)))

            primary = step.get("expect_primary")
            if primary and primary["kind"] != "none":
                match = [s for s in seen if s == (primary["point"], primary["kind"])]
                if not match:
                    problems.append("%s/%s: 主告警 %s/%s 在之前的步骤里没有出现过"
                                    % (case["id"], step["label"],
                                       primary["point"], primary["kind"]))
    return problems


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--table", default=str(DEFAULT_TABLE))
    parser.add_argument("--json", default=str(DEFAULT_JSON))
    parser.add_argument("--header", default=str(DEFAULT_HEADER))
    parser.add_argument("--list-dump", default=None,
                        help="板端 vgpoint list 输出文件，用来核对点表是否已变化")
    parser.add_argument("--check", action="store_true",
                        help="只校验已提交产物是否与重新生成的结果一致")
    args = parser.parse_args()

    table_path = Path(args.table).resolve()
    doc, points = load_table(table_path)

    if args.list_dump:
        problems = compare_with_dump(points, Path(args.list_dump).read_text(
            encoding="utf-8", errors="replace"))
        if problems:
            for problem in problems:
                print("点表不一致: %s" % problem, file=sys.stderr)
            print("板端点表已变化，请重新核对阈值并重新生成本任务的数据。",
                  file=sys.stderr)
            return 1
        print("点表核对通过：板端 vgpoint list 与 %s 一致"
              % table_path.relative_to(REPO_ROOT))

    cases = build_cases(points)
    annotate_sample_history(points, cases)
    problems = check_expectations(points, cases)
    if problems:
        for problem in problems:
            print("自检失败: %s" % problem, file=sys.stderr)
        return 1

    gaps = build_gaps(points)
    scenario_doc = build_doc(points, cases, gaps, table_path)
    json_text = json.dumps(scenario_doc, ensure_ascii=False, indent=2) + "\n"

    header_path = Path(args.header).resolve()
    json_path = Path(args.json).resolve()

    if args.check:
        stale = []
        if not json_path.exists() or json_path.read_text(encoding="utf-8") != json_text:
            stale.append(str(json_path))
        tmp_header = header_path.with_suffix(".h.new")
        emit_header(points, cases, tmp_header)
        if (not header_path.exists()
                or header_path.read_text(encoding="utf-8") != tmp_header.read_text(encoding="utf-8")):
            stale.append(str(header_path))
        tmp_header.unlink(missing_ok=True)
        if stale:
            for path in stale:
                print("产物不是最新: %s" % path, file=sys.stderr)
            print("请重新运行 %s" % Path(__file__).name, file=sys.stderr)
            return 1
        print("产物校验通过：scenarios.json 与 alarm_scenarios_fixture.h 都是最新的")
        return 0

    json_path.parent.mkdir(parents=True, exist_ok=True)
    json_path.write_text(json_text, encoding="utf-8")
    emit_header(points, cases, header_path)

    steps = sum(len(case["steps"]) for case in cases)
    print("生成 %s：%d 个工况，%d 个步骤" % (json_path.name, len(cases), steps))
    print("生成 %s" % header_path.name)
    if gaps:
        print("测试前置缺口 %d 项（见 scenarios.json 的 gaps）" % len(gaps))
    return 0


if __name__ == "__main__":
    sys.exit(main())
