#!/usr/bin/env python3
"""Static and artifact checks for VelaGuard UI performance configuration."""

from __future__ import annotations

import argparse
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8")
    except FileNotFoundError:
        return ""


class Checks:
    def __init__(self) -> None:
        self.failed = 0

    def check(self, condition: bool, label: str) -> None:
        print(f"{'PASS' if condition else 'FAIL'}: {label}")
        if not condition:
            self.failed += 1


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--artifacts", type=Path)
    args = parser.parse_args()

    checks = Checks()
    kconfig = read(ROOT / "app/velaguard_app/Kconfig")
    cmake = read(ROOT / "app/velaguard_app/CMakeLists.txt")
    makefile = read(ROOT / "app/velaguard_app/Makefile")
    build = read(ROOT / "scripts/windows_build_openvela.ps1")
    flash = read(ROOT / "scripts/windows_flash_cube.ps1")
    wrapper = read(ROOT / "scripts/windows_flash_openocd.ps1")
    patch_helper = read(ROOT / "scripts/apply-openvela-ui-performance-patch.sh")
    patch = read(ROOT / "scripts/openvela-ui-performance-stm32h750b-dk.patch")
    perf_c = read(ROOT / "app/velaguard_app/src/vg_ui_perf.c")
    perf_h = read(ROOT / "app/velaguard_app/src/vg_ui_perf.h")
    theme = read(ROOT / "app/velaguard_app/src/vg_ui_theme.c")
    board_touch = read(
        ROOT.parent / "nuttx/boards/arm/stm32h7/stm32h750b-dk/src/stm32_ft5x06.c"
    )

    checks.check(
        "VG_UI_PERF_DIAGNOSTICS" in kconfig
        and "default n" in kconfig
        and "vg_ui_perf.c" in cmake
        and "vg_ui_perf.c" in makefile,
        "diagnostics are opt-in and included by both build systems",
    )
    checks.check(
        "UiPerfDiagnostics" in build
        and "UiPerfDiagnostics" in flash
        and "UiPerfDiagnostics" in wrapper
        and "CONFIG_VG_UI_PERF_DIAGNOSTICS" in build,
        "diagnostic switch crosses every Windows build/flash boundary",
    )
    checks.check(
        "FastTouchPoll" in build
        and "FastTouchPoll" in flash
        and "FastTouchPoll" in wrapper
        and "touch_mode=%s" in build,
        "fast-poll fallback crosses every build/flash boundary",
    )
    checks.check(
        "CONFIG_LV_DEF_REFR_PERIOD 16" in build
        and "CONFIG_LV_NUTTX_VSYNC_TIMER_PERIOD 16" in build
        and "CONFIG_LVX_VELAGUARD_PRIORITY 120" in build,
        "LVGL period and UI priority are explicitly configured",
    )
    checks.check(
        "CONFIG_DEBUG_CUSTOMOPT" in build
        and "CONFIG_DEBUG_OPTLEVEL '-Og'" in build
        and "CONFIG_DEBUG_NOOPT" in build,
        "debug builds use symbols with -Og instead of no optimization",
    )
    checks.check(
        "--enable CONFIG_VG_UI_PERF_DIAGNOSTICS" in build
        and "--disable CONFIG_VG_UI_PERF_DIAGNOSTICS" in build
        and "--set-val CONFIG_VG_UI_PERF_DIAGNOSTICS" not in build,
        "bool diagnostics use Kconfig enable/disable operations",
    )
    checks.check(
        "INTERRUPT_TOUCH" in build
        and "--disable CONFIG_FT5X06_POLLMODE" in build
        and "--enable CONFIG_FT5X06_POLLMODE" in build
        and "MSEC2TICK(10)" in patch
        and "MSEC2TICK(20)" in patch
        and "Board-validated FT5336 bus rate" in patch
        and "apply --reverse --check" in patch_helper,
        "touch modes, validated I2C rate, and idempotent patching are wired",
    )
    checks.check(
        "LV_EVENT_REFR_START" in perf_c
        and "LV_EVENT_REFR_READY" in perf_c
        and "lv_anim_start" in perf_c
        and "CONFIG_VG_UI_PERF_DIAGNOSTICS" in perf_c
        and "vg_ui_perf_init" in perf_h,
        "diagnostics measure refreshes and create a test-only animation",
    )
    checks.check(
        "vg_ui_label_set_text_if_changed" in theme
        and "vg_ui_label_set_color_if_changed" in theme,
        "UI exposes shared change-detection helpers",
    )
    checks.check(
        "stm32_gpiosetevent(GPIO_FT5X06_INT, false, true, true" in board_touch,
        "FT5X06 interrupt path uses the active-low falling edge",
    )

    if args.artifacts:
        config = read(args.artifacts / "nuttx.config")
        info = read(args.artifacts / "build-info.txt")
        checks.check(
            "CONFIG_LV_DEF_REFR_PERIOD=16" in config
            and "CONFIG_LV_NUTTX_VSYNC_TIMER_PERIOD=16" in config
            and "CONFIG_LVX_VELAGUARD_PRIORITY=120" in config,
            "artifact contains optimized LVGL scheduling",
        )
        checks.check(
            "CONFIG_DEBUG_NOOPT is not set" in config
            or "CONFIG_DEBUG_CUSTOMOPT=y" in config,
            "artifact does not select debug no-opt exclusively",
        )
        checks.check(
            ("ui_perf=disabled" in info)
            or ("ui_perf=enabled" in info and "CONFIG_VG_UI_PERF_DIAGNOSTICS=y" in config),
            "artifact metadata matches diagnostic configuration",
        )

    return checks.failed


if __name__ == "__main__":
    raise SystemExit(main())
