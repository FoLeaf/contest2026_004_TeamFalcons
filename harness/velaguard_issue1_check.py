#!/usr/bin/env python3
"""Static and artifact checks for VelaGuard ISSUE1."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
OPENVELA_ROOT = REPO_ROOT.parent
APP_ROOT = REPO_ROOT / "app/velaguard_app"


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


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--artifacts",
        type=Path,
        help="optionally validate VelaGuard metadata and ELF symbols",
    )
    args = parser.parse_args()

    checks = Checks()
    kconfig = read(APP_ROOT / "Kconfig")
    cmake = read(APP_ROOT / "CMakeLists.txt")
    main_source = read(APP_ROOT / "velaguard_main.c")
    identity = read(APP_ROOT / "src/vg_identity.c")
    identity_header = read(APP_ROOT / "src/vg_identity.h")
    startup = read(APP_ROOT / "src/vg_startup.c")
    startup_header = read(APP_ROOT / "src/vg_startup.h")
    ui = read(APP_ROOT / "src/vg_ui_home.c")
    manifest = read(REPO_ROOT / "contest2026_004_TeamFalcons.xml")
    link_helper = read(REPO_ROOT / "scripts/ensure-openvela-links.sh")
    win_build = read(REPO_ROOT / "scripts/windows_build_openvela.ps1")
    win_flash = read(REPO_ROOT / "scripts/windows_flash_cube.ps1")

    checks.check(
        all(
            (APP_ROOT / path).is_file()
            for path in (
                "Kconfig",
                "CMakeLists.txt",
                "Make.defs",
                "Makefile",
                "velaguard_main.c",
                "src/vg_identity.c",
                "src/vg_identity.h",
                "src/vg_startup.c",
                "src/vg_startup.h",
                "src/vg_ui_home.c",
                "src/vg_ui_home.h",
            )
        ),
        "VelaGuard application owns all ISSUE1 modules",
    )
    checks.check(
        "CONFIG_LVX_USE_VELAGUARD" in cmake
        and "NAME velaguard" in cmake
        and "DEPENDS lvgl" in cmake
        and "LVX_USE_VELAGUARD" in kconfig
        and "VG_BUILD_MODE" in kconfig
        and "VG_DEVICE_ID_OVERRIDE" in kconfig,
        "Kconfig and CMake expose product and compiler-independent app config",
    )
    checks.check(
        "nsh_initialize()" in main_source
        and "task_create(\"velaguard_ui\"" in main_source
        and "nsh_consolemain(argc, argv)" in main_source
        and "lv_nuttx_dsc_init(&info)" in main_source
        and "info.fb_path" not in main_source
        and "result.indev == NULL" in main_source,
        "entrypoint preserves board init, framebuffer choice, UI task, and NSH",
    )
    checks.check(
        "0x1ff1e800u" in identity.lower()
        and "CONFIG_VG_BUILD_MODE == 0" in identity
        and "CONFIG_VG_DEVICE_ID_OVERRIDE" in identity
        and "vg-" in identity
        and "vg_device_id_character_valid" in identity
        and "vg_identity_valid" in identity_header
        and "set_device" not in identity.lower(),
        "identity validates test override and derives production ID from UID",
    )
    checks.check(
        all(
            value in startup
            for value in (
                '"/data/velaguard"',
                '"/data/velaguard/configs"',
                '"/data/velaguard/logs"',
                '"/data/velaguard/logs/latest.log"',
                '"/data/velaguard/logs/events.jsonl"',
                '"/dev/urandom"',
                '\\"ts_ms\\"',
                '\\"uptime_ms\\"',
                '\\"time_quality\\"',
                '\\"boot_id\\"',
            )
        )
        and "directories_ready" in startup_header
        and "human_log_written" in startup_header
        and "event_written" in startup_header
        and "storage_error" in startup_header
        and "vg_json_string" in startup,
        "startup owns directories, escaped JSONL, timestamps, and degradation",
    )
    checks.check(
        all(
            value in ui.lower()
            for value in (
                "0x0c1218",
                "0x151e26",
                "0x1c2731",
                "0xe7edf2",
                "0x8d9aa5",
                "0x39b6b2",
                "0xe05b5b",
                "0xd7a84a",
            )
        )
        and "VG_PANEL_RADIUS      8" in ui
        and "VG_CARD_COUNT" not in ui
        and "gradient" not in ui.lower()
        and "glow" not in ui.lower(),
        "Taste tokens use one accent, semantic colors, and one radius system",
    )
    checks.check(
        all(
            text in ui
            for text in (
                "VelaGuard",
                "LOCAL GATEWAY",
                "ACQUISITION",
                "Not configured",
                "ALARM",
                "Not armed",
                "NETWORK",
                "Offline",
                "AUDIO",
                "Unavailable",
                "TIME",
                "Unsynced",
                "Storage: %s",
                "IDENTITY ERROR",
            )
        )
        and "vg_ui_home_uptime_checkpoint" in ui
        and "__attribute__((noinline))" in ui,
        "home screen exposes honest ISSUE1 states and a stable breakpoint",
    )
    checks.check(
        "app/velaguard_app" in manifest
        and "contest2026_004_velaguard_app" in manifest
        and "ln -sfn" in link_helper
        and "Refusing to replace non-symlink" in link_helper,
        "manifest and guarded helper expose only the contest-owned app link",
    )
    checks.check(
        'ValidateSet("test", "production")' in win_build
        and "VelaGuardMode" in win_build
        and "DeviceIdOverride" in win_build
        and "CONFIG_LVX_USE_VELAGUARD" in win_build
        and "CONFIG_LVX_USE_DEMO_CONTEST2026_004_VSCODE_LAB" in win_build
        and "CONFIG_VG_BUILD_MODE" in win_build
        and "CONFIG_INIT_ENTRYPOINT 'velaguard_main'" in win_build
        and 'apps/tools/mkkconfig.sh' in win_build
        and "nuttx.config" in win_build
        and "build-info.txt" in win_build
        and 'app/velaguard_app/"*.o' in win_build
        and 'app/velaguard_app/src/"*.o' in win_build
        and "VelaGuardMode = $VelaGuardMode" in win_flash
        and "DeviceIdOverride = $DeviceIdOverride" in win_flash,
        "Windows workflow separates product mode from debug optimization",
    )

    visible_sources = "\n".join((ui, main_source))
    checks.check(
        "—" not in visible_sources and "–" not in visible_sources,
        "embedded UI contains no Taste-forbidden dash characters",
    )

    try:
        tasks = json.loads(read(REPO_ROOT / ".vscode/tasks.json"))
        launches = json.loads(read(REPO_ROOT / ".vscode/launch.json"))
    except json.JSONDecodeError as error:
        checks.check(False, "VS Code JSON parses", str(error))
    else:
        labels = {task.get("label") for task in tasks.get("tasks", [])}
        checks.check(
            {
                "openvela: build VelaGuard test QSPI firmware",
                "openvela: build VelaGuard test debug QSPI firmware",
                "openvela: build VelaGuard production QSPI firmware",
                "openvela: flash VelaGuard production firmware (CubeProgrammer)",
            }.issubset(labels)
            and all(
                launch.get("request") == "attach"
                and launch.get("loadFiles") == []
                for launch in launches.get("configurations", [])
            ),
            "VS Code exposes the build matrix and attach-only debugging",
        )

    if args.artifacts:
        config = read(args.artifacts / "nuttx.config")
        build_info = read(args.artifacts / "build-info.txt")
        checks.check(
            "CONFIG_LVX_USE_VELAGUARD=y" in config
            and 'CONFIG_INIT_ENTRYPOINT="velaguard_main"' in config
            and "CONFIG_STM32H750B_DK_QSPI_BOOT=y" in config,
            "artifact config selects VelaGuard QSPI boot",
        )
        checks.check(
            ("product_mode=test" in build_info and "CONFIG_VG_BUILD_MODE=0" in config)
            or (
                "product_mode=production" in build_info
                and "CONFIG_VG_BUILD_MODE=1" in config
            ),
            "artifact product-mode metadata matches Kconfig",
        )

        nm = (
            OPENVELA_ROOT
            / "prebuilts/gcc/linux-x86_64/arm-none-eabi/bin/arm-none-eabi-nm"
        )
        elf = args.artifacts / "nuttx.elf"
        if nm.is_file() and elf.is_file():
            symbols = subprocess.run(
                [str(nm), "-g", str(elf)],
                check=False,
                capture_output=True,
                text=True,
            ).stdout
            checks.check(
                all(
                    symbol in symbols
                    for symbol in (
                        "velaguard_main",
                        "vg_identity_init",
                        "vg_startup_record",
                        "vg_ui_home_uptime_checkpoint",
                        "g_velaguard_uptime_seconds",
                    )
                ),
                "ELF exports VelaGuard entry and debug symbols",
            )
        else:
            checks.check(False, "artifact ELF and arm-none-eabi-nm exist")

    return 1 if checks.failed else 0


if __name__ == "__main__":
    sys.exit(main())
