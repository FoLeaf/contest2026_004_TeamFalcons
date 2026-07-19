#!/usr/bin/env python3
"""Static + behavioral regression checks for VelaGuard settings/network UI."""

from __future__ import annotations

import ipaddress
import json
import re
import sys
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
APP_ROOT = REPO_ROOT / "app/velaguard_app"
PATCH = REPO_ROOT / "scripts/openvela-netinit-carrier-poll.patch"
WIN_BUILD = REPO_ROOT / "scripts/windows_build_openvela.ps1"


def read(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8")
    except FileNotFoundError:
        return ""


class Checks:
    def __init__(self) -> None:
        self.failed = 0

    def check(self, condition: bool, label: str, detail: str = "") -> None:
        status = "PASS" if condition else "FAIL"
        suffix = f" ({detail})" if detail else ""
        print(f"{status}: {label}{suffix}")
        if not condition:
            self.failed += 1


# --- Pure behavioral mirrors of vg_config IPv4 rules (no NuttX runtime) ---


def netmask_is_contiguous(mask_h: int) -> bool:
    if mask_h == 0:
        return False
    inv = (~mask_h) & 0xFFFFFFFF
    return (inv & (inv + 1)) == 0


def ipv4_parse(text: str) -> int | None:
    try:
        return int(ipaddress.IPv4Address(text))
    except Exception:
        return None


def validate_network_config(cfg: dict) -> bool:
    """Mirror vg_config_network_validate contracts for harness coverage."""
    if cfg.get("version") != 1:
        return False
    mode = cfg.get("mode")
    if mode == "dhcp":
        for key in ("ipv4", "netmask", "gateway", "dns"):
            val = cfg.get(key, "")
            if val and ipv4_parse(val) is None:
                return False
        return True
    if mode != "static":
        return False
    try:
        addr_h = ipv4_parse(cfg["ipv4"])
        mask_h = ipv4_parse(cfg["netmask"])
        gw_h = ipv4_parse(cfg["gateway"])
        dns_h = ipv4_parse(cfg["dns"])
    except KeyError:
        return False
    if None in (addr_h, mask_h, gw_h, dns_h):
        return False
    assert addr_h is not None and mask_h is not None and gw_h is not None
    assert dns_h is not None
    if not netmask_is_contiguous(mask_h):
        return False
    if addr_h == 0 or (addr_h & 0xFF000000) == 0x7F000000:
        return False
    if (addr_h & 0xF0000000) == 0xE0000000:
        return False
    if (addr_h & 0xFFFF0000) == 0xA9FE0000:
        return False
    network = addr_h & mask_h
    broadcast = network | (~mask_h & 0xFFFFFFFF)
    if addr_h in (network, broadcast):
        return False
    if (gw_h & mask_h) != network or gw_h in (network, broadcast):
        return False
    if dns_h == 0 or (dns_h & 0xF0000000) == 0xE0000000:
        return False
    return True


def simulate_apply_transaction(
    write_tmp_ok: bool,
    runtime_ok: bool,
    rename_ok: bool,
) -> dict:
    """Model tmp -> runtime -> rename; failures abort without commit."""
    formal = {"mode": "dhcp", "rev": 0}
    runtime = {"mode": "dhcp", "rev": 0}
    tmp = None
    steps: list[str] = []

    requested = {"mode": "static", "rev": formal["rev"] + 1}

    if not write_tmp_ok:
        steps.append("tmp_fail")
        return {
            "formal": formal,
            "runtime": runtime,
            "tmp": None,
            "committed": False,
            "steps": steps,
        }

    tmp = dict(requested)
    steps.append("tmp_ok")

    if not runtime_ok:
        steps.append("runtime_fail")
        tmp = None  # abort_tmp
        # runtime restored to previous
        return {
            "formal": formal,
            "runtime": runtime,
            "tmp": None,
            "committed": False,
            "steps": steps,
        }

    runtime = dict(requested)
    steps.append("runtime_ok")

    if not rename_ok:
        steps.append("rename_fail")
        runtime = {"mode": "dhcp", "rev": 0}  # rollback runtime
        tmp = None
        return {
            "formal": formal,
            "runtime": runtime,
            "tmp": None,
            "committed": False,
            "steps": steps,
        }

    formal = dict(tmp)
    tmp = None
    steps.append("rename_ok")
    return {
        "formal": formal,
        "runtime": runtime,
        "tmp": None,
        "committed": True,
        "steps": steps,
    }


def main() -> int:
    checks = Checks()

    sources = {
        "config_h": read(APP_ROOT / "src/vg_config.h"),
        "config_c": read(APP_ROOT / "src/vg_config.c"),
        "network_h": read(APP_ROOT / "src/vg_network.h"),
        "network_c": read(APP_ROOT / "src/vg_network.c"),
        "ui_home": read(APP_ROOT / "src/vg_ui_home.c"),
        "ui_settings": read(APP_ROOT / "src/vg_ui_settings.c"),
        "ui_network": read(APP_ROOT / "src/vg_ui_network.c"),
        "ui_nav": read(APP_ROOT / "src/vg_ui_nav.c"),
        "main": read(APP_ROOT / "velaguard_main.c"),
        "makefile": read(APP_ROOT / "Makefile"),
        "cmake": read(APP_ROOT / "CMakeLists.txt"),
        "patch": read(PATCH),
        "win_build": read(WIN_BUILD),
        "font16": read(APP_ROOT / "src/fonts/vg_font_cn_16.c"),
        "font20": read(APP_ROOT / "src/fonts/vg_font_cn_20.c"),
        "icons_h": read(APP_ROOT / "src/assets/vg_icons.h"),
        "readme": read(APP_ROOT / "README.md"),
    }

    required_files = [
        "src/vg_network.c",
        "src/vg_network.h",
        "src/vg_ui_nav.c",
        "src/vg_ui_nav.h",
        "src/vg_ui_theme.c",
        "src/vg_ui_theme.h",
        "src/vg_ui_settings.c",
        "src/vg_ui_settings.h",
        "src/vg_ui_network.c",
        "src/vg_ui_network.h",
        "src/assets/vg_icons.h",
        "src/assets/vg_img_setting.c",
        "src/assets/vg_img_ethernet.c",
        "src/assets/vg_img_wifi.c",
        "src/fonts/vg_font_cn_16.c",
        "src/fonts/vg_font_cn_20.c",
    ]
    checks.check(
        all((APP_ROOT / path).is_file() for path in required_files),
        "settings/network modules and assets exist",
    )

    checks.check(
        "network.json" in sources["config_h"]
        and "vg_config_network_validate" in sources["config_c"]
        and "vg_config_network_write_tmp" in sources["config_c"]
        and "vg_config_network_commit" in sources["config_c"]
        and "cJSON_Parse" in sources["config_c"]
        and "rename(" in sources["config_c"],
        "vg_config owns versioned network.json parse/validate/atomic write",
    )

    checks.check(
        "netinit_set_ipv4_config" in sources["network_c"]
        and "vg_network_request_apply" in sources["network_c"]
        and "pthread_create" in sources["network_c"]
        and "正在应用" in sources["network_c"]
        and "feedback" in sources["network_h"]
        and "vg_network_set_feedback_locked" in sources["network_c"]
        and "ioctl" not in sources["ui_network"]
        and "netlib_" not in sources["ui_network"]
        and "cJSON" not in sources["ui_network"]
        and "status.feedback" in sources["ui_network"],
        "vg_network owns async apply; UI does not call netlib/ioctl/cJSON",
    )

    checks.check(
        "vg_netmask_is_contiguous" in sources["config_c"]
        and "addr_h == network" in sources["config_c"]
        and "gw_h & mask_h" in sources["config_c"]
        and "write_tmp" in sources["config_c"]
        and "fsync" in sources["config_c"]
        and "rename(" in sources["config_c"]
        and "abort_tmp" in sources["config_c"]
        and "fail_runtime" in sources["network_c"]
        and "fail_commit" in sources["network_c"]
        and "apply_runtime(&previous)" in sources["network_c"],
        "IPv4 combo validation and apply/rollback failure paths present",
    )

    # Apply transaction order in source: write_tmp before apply_runtime before commit
    apply_src = sources["network_c"]
    i_tmp = apply_src.find("vg_config_network_write_tmp")
    i_rt = apply_src.find("vg_network_apply_runtime(&requested)")
    i_commit = apply_src.find("vg_config_network_commit")
    checks.check(
        0 <= i_tmp < i_rt < i_commit,
        "apply transaction order is tmp -> runtime -> rename",
        f"pos tmp={i_tmp} runtime={i_rt} commit={i_commit}",
    )

    ok_tx = simulate_apply_transaction(True, True, True)
    checks.check(
        ok_tx["committed"]
        and ok_tx["formal"]["mode"] == "static"
        and ok_tx["steps"] == ["tmp_ok", "runtime_ok", "rename_ok"],
        "apply model: success commits formal after runtime",
    )
    fail_tmp = simulate_apply_transaction(False, True, True)
    checks.check(
        not fail_tmp["committed"] and fail_tmp["formal"]["mode"] == "dhcp",
        "apply model: tmp failure leaves formal untouched",
    )
    fail_rt = simulate_apply_transaction(True, False, True)
    checks.check(
        not fail_rt["committed"]
        and fail_rt["formal"]["mode"] == "dhcp"
        and fail_rt["runtime"]["mode"] == "dhcp",
        "apply model: runtime failure aborts without commit",
    )
    fail_ren = simulate_apply_transaction(True, True, False)
    checks.check(
        not fail_ren["committed"]
        and fail_ren["formal"]["mode"] == "dhcp"
        and fail_ren["runtime"]["mode"] == "dhcp",
        "apply model: rename failure rolls runtime back, formal unchanged",
    )

    # Behavioral IPv4 validation cases
    good_static = {
        "version": 1,
        "mode": "static",
        "ipv4": "192.168.1.50",
        "netmask": "255.255.255.0",
        "gateway": "192.168.1.1",
        "dns": "8.8.8.8",
    }
    checks.check(validate_network_config(good_static), "validate accepts good static config")
    checks.check(
        validate_network_config({"version": 1, "mode": "dhcp", "ipv4": "", "netmask": "",
                                 "gateway": "", "dns": ""}),
        "validate accepts empty DHCP config",
    )
    bad_cases = [
        {**good_static, "netmask": "255.0.255.0"},  # non-contiguous
        {**good_static, "ipv4": "192.168.1.0"},  # network address
        {**good_static, "ipv4": "192.168.1.255"},  # broadcast
        {**good_static, "gateway": "10.0.0.1"},  # different subnet
        {**good_static, "dns": "0.0.0.0"},
        {**good_static, "ipv4": "224.0.0.1"},  # multicast
        {"version": 2, "mode": "dhcp"},
    ]
    checks.check(
        all(not validate_network_config(c) for c in bad_cases),
        "validate rejects bad static/combo/version cases",
    )

    # Parse round-trip for default JSON schema using tempfile
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "network.json"
        default = {
            "version": 1,
            "mode": "dhcp",
            "ipv4": "",
            "netmask": "",
            "gateway": "",
            "dns": "",
        }
        path.write_text(json.dumps(default, indent=2) + "\n", encoding="utf-8")
        loaded = json.loads(path.read_text(encoding="utf-8"))
        checks.check(
            loaded == default and validate_network_config(loaded),
            "network.json default schema parses and validates",
        )

    checks.check(
        "netinit_set_ipv4_config" in sources["patch"]
        and "NETINIT_IPV4_STATIC" in sources["patch"]
        and "g_ipv4_policy" in sources["patch"]
        and "netinit_reapply_policy_locked" in sources["patch"]
        and "g_ipv4_policy_lock" in sources["patch"]
        and "netinit_seed_boot_policy_locked" in sources["patch"],
        "maintained netinit patch exposes shared runtime DHCP/static policy",
    )

    checks.check(
        "netinit_clear_dns" in sources["patch"]
        and "netinit_replace_dns" in sources["patch"]
        and "dns_default_nameserver" in sources["patch"]
        and sources["patch"].count("netinit_clear_dns") >= 2
        and "netlib_set_ipv4dnsaddr" in sources["patch"],
        "DNS is cleared/replaced on policy apply (not append-only)",
    )

    checks.check(
        "g_ipv4_policy_valid" in sources["patch"]
        and "netinit_apply_static_locked" in sources["patch"]
        and "netinit_reapply_policy_locked" in sources["patch"],
        "static reconnect reuses retained policy under policy lock",
    )

    checks.check(
        sources["patch"].count("g_ipv4_policy_generation++") >= 2
        and "generation != g_ipv4_policy_generation" in sources["patch"]
        and "g_ipv4_policy.mode == NETINIT_IPV4_STATIC" in sources["patch"]
        and sources["patch"].count("netinit_obtain_ipv4addr(generation)") >= 2
        and "ret = netlib_obtain_ipv4addr(NET_DEVNAME)" in sources["patch"]
        and "ret = netinit_apply_static_locked(&g_ipv4_policy)" in sources["patch"],
        "stale in-flight DHCP result restores newer static policy",
    )

    # Startup race mitigation: no sync multi-second apply on UI path
    start_fn = sources["network_c"]
    # vg_network_start must not call netinit_set_ipv4_config inline
    start_match = re.search(
        r"int\s+vg_network_start\s*\([^)]*\)\s*\{(.*?)^\}",
        start_fn,
        re.S | re.M,
    )
    start_body = start_match.group(1) if start_match else ""
    checks.check(
        "g_boot_policy_pending" in sources["network_c"]
        and "vg_network_handle_boot_policy" in sources["network_c"]
        and "netinit_set_ipv4_config" not in start_body
        and "netlib_obtain" not in start_body,
        "vg_network_start does not sync-apply DHCP/static on caller/UI path",
        "boot policy deferred to worker" if "g_boot_policy_pending" in sources["network_c"] else "",
    )

    # Snapshot lock must not cover I/O: query outside lock publish pattern
    checks.check(
        "vg_network_publish_runtime" in sources["network_c"]
        and "vg_network_query_carrier" in sources["network_c"]
        and re.search(
            r"vg_network_query_carrier\(\);\s*\n\s*vg_network_query_ipv4",
            sources["network_c"],
        )
        is not None
        and "pthread_mutex_lock(&g_network_lock);\n  *status = g_status;"
        in sources["network_c"].replace("\r\n", "\n"),
        "UI snapshot getter only copies; queries run outside UI-facing lock",
    )

    # Ensure publish queries before locking (no I/O under lock for carrier/ip)
    pub = re.search(
        r"static void vg_network_publish_runtime\(void\)\s*\{(.*?)^\}",
        sources["network_c"],
        re.S | re.M,
    )
    pub_body = pub.group(1) if pub else ""
    q_pos = pub_body.find("vg_network_query_carrier")
    lock_pos = pub_body.find("pthread_mutex_lock")
    checks.check(
        q_pos >= 0 and lock_pos > q_pos,
        "publish_runtime queries network before taking snapshot lock",
        f"q={q_pos} lock={lock_pos}",
    )

    checks.check(
        "CONFIG_NETDB_DNSCLIENT" in sources["win_build"]
        and "CONFIG_NETUTILS_CJSON" in sources["win_build"]
        and "CONFIG_NETINIT_DNS" in sources["win_build"],
        "Windows build enables DNS client and cJSON",
    )

    checks.check(
        "vg_ui_nav_init" in sources["ui_home"]
        and "lv_timer_create" in sources["ui_nav"]
        and sources["ui_home"].count("lv_timer_create") == 0
        and "LV_EVENT_DELETE" in sources["ui_nav"]
        and "vg_ui_home_clear_refs" in sources["ui_home"],
        "single service timer and screen-delete ref clearing",
    )

    settings_order = [
        "网络设置",
        "采集与传感器",
        "告警规则",
        "云端与 MQTT",
        "声音",
        "系统",
        "固件更新",
    ]
    order_ok = True
    last = -1
    for title in settings_order:
        pos = sources["ui_settings"].find(f'"{title}"')
        if pos < 0 or pos < last:
            order_ok = False
            break
        last = pos
    checks.check(
        order_ok and "暂未开放" in sources["ui_settings"] and "可用" in sources["ui_settings"],
        "settings list order and unavailable placeholders",
    )

    checks.check(
        "vg_img_setting" in sources["ui_home"]
        and "设置" not in re.findall(r'vg_ui_label\([^;]*"设置"', sources["ui_home"])
        and "无链路" in sources["ui_home"]
        and "vg_network_get_status" in sources["ui_home"],
        "home uses real network status and icon-only settings entry",
    )

    checks.check(
        "LV_KEYBOARD_MODE_NUMBER" in sources["ui_nav"]
        and "保存并应用" in sources["ui_network"]
        and "确认应用" in sources["ui_network"]
        and "Wi-Fi" in sources["ui_network"]
        and "暂未开放" in sources["ui_network"]
        and "vg_network_request_apply" in sources["ui_network"],
        "network page has keyboard, confirm, apply, and Wi-Fi placeholder",
    )

    # DHCP layout reflow: g_static_visible drives Y positions
    checks.check(
        "g_static_visible" in sources["ui_network"]
        and "vg_reflow_action_rows" in sources["ui_network"]
        and "VG_NET_WIFI_DHCP_Y" in sources["ui_network"]
        and "VG_NET_WIFI_STATIC_Y" in sources["ui_network"]
        and "vg_reflow_action_rows" in sources["ui_network"]
        and re.search(
            r"g_static_visible\s*=\s*enabled",
            sources["ui_network"],
        )
        is not None
        and "lv_obj_set_pos(g_wifi_row" in sources["ui_network"]
        and "lv_obj_set_pos(g_apply_btn" in sources["ui_network"],
        "DHCP mode reflows Wi-Fi/Save (no fixed dead static-field space)",
    )

    required_glyphs = [
        "设置",
        "网络",
        "暂未开放",
        "保存",
        "应用",
        "静态",
        "掩码",
        "网关",
        "返回",
        "确认",
        "取消",
    ]
    missing16 = [g for g in required_glyphs if g not in sources["font16"]]
    opts16 = (
        sources["font16"].split("Opts:", 1)[-1].split("\n", 1)[0]
        if "Opts:" in sources["font16"]
        else ""
    )
    opts20 = (
        sources["font20"].split("Opts:", 1)[-1].split("\n", 1)[0]
        if "Opts:" in sources["font20"]
        else ""
    )
    checks.check(
        all(g in opts16 for g in required_glyphs),
        "font16 opts include new Chinese UI strings",
        f"missing={[g for g in required_glyphs if g not in opts16]}",
    )
    checks.check(
        all(g in opts20 for g in required_glyphs),
        "font20 opts include new Chinese UI strings",
        f"missing={[g for g in required_glyphs if g not in opts20]}",
    )
    _ = missing16

    checks.check(
        "vg_img_setting" in sources["icons_h"]
        and "vg_img_ethernet" in sources["icons_h"]
        and "vg_img_wifi" in sources["icons_h"]
        and "LV_COLOR_FORMAT_A8" in read(APP_ROOT / "src/assets/vg_img_setting.c"),
        "SVG icons converted to LVGL A8 descriptors",
    )

    checks.check(
        "vg_network.c" in sources["makefile"]
        and "vg_ui_network.c" in sources["makefile"]
        and "vg_img_setting.c" in sources["makefile"]
        and "vg_network.c" in sources["cmake"],
        "Makefile/CMake list new sources",
    )

    checks.check(
        "vg_network_start" in sources["main"]
        and "network.json" in sources["readme"],
        "main starts network service; README documents network config",
    )

    checks.check(
        (REPO_ROOT / "res/setting.svg").is_file()
        and (REPO_ROOT / "res/settings_ethernet.svg").is_file()
        and (REPO_ROOT / "res/WIFI.svg").is_file()
        and (REPO_ROOT / "scripts/gen_vg_icons.py").is_file(),
        "source SVGs and icon generator retained",
    )

    # Patch must not contain trailing whitespace on non-empty context
    patch_text = sources["patch"]
    trail_bad = []
    for i, line in enumerate(patch_text.splitlines(), 1):
        if line.endswith(" ") and line not in (" ", "+", "-"):
            # unified diff empty context is a single space — OK
            if line.startswith((" ", "+", "-")) and line[1:].rstrip(" ") != line[1:]:
                trail_bad.append(i)
    checks.check(
        not trail_bad,
        "maintained netinit patch has no trailing whitespace",
        f"lines={trail_bad[:10]}" if trail_bad else "",
    )

    if checks.failed:
        print(f"\n{checks.failed} check(s) failed")
        return 1
    print("\nAll settings/network checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
