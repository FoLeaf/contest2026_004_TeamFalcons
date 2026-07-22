#!/usr/bin/env python3
"""Static and artifact checks for VelaGuard DMA2D display acceleration."""

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
    build = read(ROOT / "scripts/windows_build_openvela.ps1")
    flash = read(ROOT / "scripts/windows_flash_cube.ps1")
    wrapper = read(ROOT / "scripts/windows_flash_openocd.ps1")
    app_kconfig = read(ROOT / "app/velaguard_app/Kconfig")
    app_cmake = read(ROOT / "app/velaguard_app/CMakeLists.txt")
    app_make = read(ROOT / "app/velaguard_app/Makefile")
    dma2d = read(ROOT / "app/velaguard_app/src/vg_ui_dma2d.c")
    patch = read(
        ROOT / "scripts/openvela-display-acceleration-stm32h750b-dk.patch"
    )
    helper = read(ROOT / "scripts/apply-openvela-display-acceleration-patch.sh")

    checks.check(
        "VG_STM32H7_DMA2D" in app_kconfig
        and "vg_ui_dma2d.c" in app_cmake
        and "vg_ui_dma2d.c" in app_make,
        "DMA2D draw unit is Kconfig controlled and built by Make/CMake",
    )
    checks.check(
        "VG_DMA2D_MIN_PIXELS       256u" in dma2d
        and "LV_DRAW_TASK_TYPE_FILL" in dma2d
        and "LV_COLOR_FORMAT_RGB565" in dma2d
        and "lv_draw_sw_fill" in dma2d,
        "DMA2D handles bounded RGB565 fills with software fallback",
    )
    checks.check(
        "up_flush_dcache" in dma2d
        and "up_invalidate_dcache" in dma2d
        and "VG_DMA2D_TIMEOUT_MS" in dma2d
        and "VG_DMA2D_CR_ABORT" in dma2d,
        "DMA2D path owns cache coherency and bounded timeout recovery",
    )
    checks.check(
        '" fallback=%" PRIu32' in dma2d
        and '" errors=%" PRIu32' in dma2d
        and "VG_DMA2D_REPORT_US" in dma2d,
        "DMA2D hardware, fallback, and error counters are observable",
    )
    checks.check(
        "BOARD_SDRAM2_HEAP_OFFSET" in patch
        and "static volatile bool g_flip_pending" in patch
        and "+  putreg32(LTDC_SRCR_VBR, STM32_LTDC_SRCR)" in patch
        and "-  ret = stm32_ltdc_reload(LTDC_SRCR_VBR, true)" in patch,
        "maintained patch reserves SDRAM and submits asynchronous VBlank flips",
    )
    checks.check(
        "apply --reverse --check" in helper
        and "apply-openvela-display-acceleration-patch.sh" in build,
        "display patch is idempotent and wired before configuration",
    )
    checks.check(
        all(name in build and name in flash and name in wrapper for name in (
            "InterruptTouch", "DisableDma2d", "HidePerfMonitor"
        )),
        "rollback switches cross every Windows build/flash boundary",
    )
    checks.check(
        "CONFIG_LV_DEF_REFR_PERIOD 16" in build
        and "CONFIG_SCHED_CPULOAD_SYSCLK" in build
        and "CONFIG_LV_USE_PERF_MONITOR" in build
        and "CONFIG_LV_PERF_MONITOR_ALIGN_TOP_LEFT" in build,
        "16 ms scheduling and visible native FPS/CPU monitor are selected",
    )
    checks.check(
        "render_backend=%s" in build
        and "perf_monitor=%s" in build
        and "touch_mode=%s" in build,
        "artifact metadata identifies rendering, monitor, and touch modes",
    )

    if args.artifacts:
        config = read(args.artifacts / "nuttx.config")
        info = read(args.artifacts / "build-info.txt")
        checks.check(
            "CONFIG_LV_DEF_REFR_PERIOD=16" in config
            and "CONFIG_SCHED_CPULOAD_SYSCLK=y" in config,
            "artifact has 16 ms scheduling and CPU-load collection",
        )
        checks.check(
            "render_backend=" in info
            and "perf_monitor=" in info
            and "touch_mode=" in info,
            "artifact exposes all performance mode metadata",
        )
        if "render_backend=dma2d" in info:
            checks.check(
                "CONFIG_VG_STM32H7_DMA2D=y" in config,
                "DMA2D artifact compiles the hardware draw unit",
            )

    return checks.failed


if __name__ == "__main__":
    raise SystemExit(main())
