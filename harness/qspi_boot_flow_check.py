#!/usr/bin/env python3
"""Static and artifact checks for the STM32H750B-DK QSPI workflow."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
OPENVELA_ROOT = REPO_ROOT.parent
NUTTX_ROOT = OPENVELA_ROOT / "nuttx"


def read(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8")
    except FileNotFoundError:
        return ""


def hex_range(path: Path) -> tuple[int, int]:
    base = 0
    low: int | None = None
    high = 0

    for line in path.read_text(encoding="ascii").splitlines():
        if not line.startswith(":"):
            continue

        count = int(line[1:3], 16)
        offset = int(line[3:7], 16)
        record_type = int(line[7:9], 16)
        if record_type == 0:
            start = base + offset
            end = start + count
            low = start if low is None else min(low, start)
            high = max(high, end)
        elif record_type == 2:
            base = int(line[9:13], 16) << 4
        elif record_type == 4:
            base = int(line[9:13], 16) << 16

    if low is None:
        raise ValueError(f"no data records in {path}")
    return low, high


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
        help="optionally validate nuttx.hex and qspi_bootstub.hex in this directory",
    )
    args = parser.parse_args()

    checks = Checks()
    stub_c = read(REPO_ROOT / "scripts/qspi_boot_stub/stm32h750b_qspi_bootstub.c")
    stub_ld = read(REPO_ROOT / "scripts/qspi_boot_stub/stm32h750b_qspi_bootstub.ld")
    stub_build = read(REPO_ROOT / "scripts/qspi_boot_stub/build_bootstub.sh")
    patch = read(REPO_ROOT / "scripts/openvela-qspi-boot-stm32h750b-dk.patch")
    patch_helper = read(REPO_ROOT / "scripts/apply-openvela-qspi-patch.sh")
    win_build = read(REPO_ROOT / "scripts/windows_build_openvela.ps1")
    win_flash = read(REPO_ROOT / "scripts/windows_flash_cube.ps1")
    openocd_wrapper = read(REPO_ROOT / "scripts/windows_flash_openocd.ps1")
    docs = read(REPO_ROOT / "docs/windows_build_debug_setup.md")
    lab_source = read(REPO_ROOT / "app/hello_app/vscode_lab_main.c")
    lab_kconfig = read(REPO_ROOT / "app/hello_app/Kconfig")
    lab_cmake = read(REPO_ROOT / "app/hello_app/CMakeLists.txt")

    checks.check(
        "APP_BASE" in stub_c
        and "0x90000000u" in stub_c
        and "SCB_VTOR" in stub_c
        and "jump_to_image" in stub_c
        and "(value & 3u) == 0" in stub_c,
        "boot stub jumps through the QSPI vector table",
    )
    checks.check(
        "0x0d003513u" in stub_c.lower()
        and "0x000003f5u" in stub_c.lower()
        and "qspi_enter_memory_mapped" in stub_c,
        "boot stub uses the hardware-validated SPI memory-mapped sequence",
    )
    checks.check(
        "ORIGIN = 0x08000000" in stub_ld
        and "LENGTH = 128K" in stub_ld
        and "ENTRY(Reset_Handler)" in stub_ld,
        "boot stub linker script is bounded by physical internal Flash",
    )
    checks.check(
        "arm-none-eabi-gcc" in stub_build
        and "qspi_bootstub.hex" in stub_build
        and "qspi_bootstub.bin" in stub_build,
        "boot stub build emits ELF, HEX, and BIN",
    )
    checks.check(
        "CONFIG_STM32H750B_DK_QSPI_BOOT" in patch
        and "ORIGIN = 0x90000000" in patch
        and "qspi_flash.ld" in patch
        and "mpu_priv_flash" in patch
        and "-if ARCH_BOARD_STM32H750B_DB" in patch
        and "+if ARCH_BOARD_STM32H750B_DK" in patch,
        "team-owned patch carries linker, Kconfig, and MPU support",
    )
    checks.check(
        "apply --reverse --check" in patch_helper
        and "apply --check" in patch_helper
        and "git -C" in patch_helper,
        "patch helper is guarded and idempotent",
    )
    checks.check(
        "TSIOC_GETMAXPOINTS" in patch
        and "CONFIG_FT5X06_SINGLEPOINT" in patch
        and "FT5X06_MAX_TOUCHES" in patch
        and "ret = -EINVAL" in patch,
        "team-owned patch keeps FT5X06 compatible with the LVGL touchscreen contract",
    )
    checks.check(
        "stm32h750b-dk:lvgl" in win_build
        and "CONFIG_STM32H750B_DK_QSPI_BOOT" in win_build
        and "CONFIG_LVX_USE_VELAGUARD" in win_build
        and "CONFIG_LVX_USE_DEMO_CONTEST2026_004_VSCODE_LAB" in win_build
        and "--disable CONFIG_EXAMPLES_LVGLDEMO" in win_build
        and "--disable CONFIG_LV_BUILD_EXAMPLES" in win_build
        and "--disable CONFIG_LV_USE_DEMO_WIDGETS" in win_build
        and "CONFIG_INIT_ENTRYPOINT 'velaguard_main'" in win_build
        and "apply-openvela-qspi-patch.sh" in win_build
        and "ensure-openvela-links.sh" in win_build
        and "app/velaguard_app/.built" in win_build
        and "qspi_bootstub.hex" in win_build
        and "--disable CONFIG_DEBUG_SYMBOLS" in win_build
        and "--enable CONFIG_DEBUG_FULLOPT" in win_build,
        "Windows build selects VelaGuard on the proven QSPI platform",
    )
    checks.check(
        "LVX_USE_DEMO_CONTEST2026_004_VSCODE_LAB" in lab_kconfig
        and "NAME vscode_lab" in lab_cmake
        and "DEPENDS lvgl" in lab_cmake
        and "vscode_lab_debug_checkpoint" in lab_source
        and "g_vscode_lab_debug_state" in lab_source
        and "__attribute__((noinline))" in lab_source
        and "nsh_initialize()" in lab_source
        and "nsh_consolemain(argc, argv)" in lab_source
        and "lv_demos" not in lab_source,
        "reference VS Code Lab retains its standalone debug contracts",
    )
    checks.check(
        "MT25TL01G_STM32H750B-DISCO.stldr" in win_flash
        and "Assert-Range \"Main QSPI image\"" in win_flash
        and "Assert-Range \"Internal boot stub\"" in win_flash
        and "ValidateOnly" in win_flash
        and "-el $ExternalLoader -d $mainHex -v" in win_flash
        and "-d $stubHex -v" in win_flash,
        "Cube flash flow validates and verifies both images",
    )
    checks.check(
        "$buildParameters = @{" in win_flash
        and "OutDir = $OutDir" in win_flash
        and '$buildParameters["DebugBuild"] = $true' in win_flash
        and "@buildParameters" in win_flash
        and "$buildArguments = @()" not in win_flash,
        "Cube flash forwards debug-build parameters by name",
    )
    checks.check(
        "windows_flash_cube.ps1" in openocd_wrapper
        and "debug-only" in openocd_wrapper,
        "legacy OpenOCD flash entry redirects to CubeProgrammer",
    )
    checks.check(
        "$parameters = @{}" in openocd_wrapper
        and all(
            f'$parameters["{name}"]' in openocd_wrapper
            for name in (
                "CubeCli",
                "ExternalLoader",
                "OutDir",
                "NoBuild",
                "DebugBuild",
                "VelaGuardMode",
                "DeviceIdOverride",
                "ValidateOnly",
            )
        )
        and "@parameters" in openocd_wrapper
        and "$arguments = @()" not in openocd_wrapper,
        "OpenOCD wrapper forwards all Cube flash parameters by name",
    )

    try:
        tasks = json.loads(read(REPO_ROOT / ".vscode/tasks.json"))
        launches = json.loads(read(REPO_ROOT / ".vscode/launch.json"))
    except json.JSONDecodeError as error:
        checks.check(False, "VSCode JSON parses", str(error))
    else:
        labels = {task.get("label") for task in tasks.get("tasks", [])}
        checks.check(
            {
                "openvela: build VelaGuard test QSPI firmware",
                "openvela: flash VelaGuard test QSPI firmware (CubeProgrammer)",
                "openvela: flash VelaGuard test debug firmware (CubeProgrammer)",
                "openvela: flash VelaGuard production firmware (CubeProgrammer)",
            }.issubset(labels),
            "VSCode exposes VelaGuard test, debug, and production tasks",
        )
        configurations = launches.get("configurations", [])
        checks.check(
            len(configurations) >= 2
            and all(config.get("request") == "attach" for config in configurations)
            and all(config.get("loadFiles") == [] for config in configurations)
            and all("/.debug/nuttx.elf" in config.get("executable", "") for config in configurations)
            and all(
                "board/stm32h750b-disco.cfg" in config.get("configFiles", [])
                for config in configurations
            ),
            "Cortex-Debug attaches through OpenOCD without loading Flash",
        )

    checks.check(
        "External Loader" in docs
        and "OpenOCD" in docs
        and "0x08000000" in docs
        and "0x90000000" in docs
        and "vg_ui_home_uptime_checkpoint" in docs,
        "Chinese workflow documentation states the tool and address boundaries",
    )

    applied_linker = read(
        NUTTX_ROOT / "boards/arm/stm32h7/stm32h750b-dk/scripts/qspi_flash.ld"
    )
    if applied_linker:
        checks.check(
            "ORIGIN = 0x90000000" in applied_linker,
            "applied NuttX checkout contains the QSPI linker script",
        )
    else:
        print("SKIP: NuttX QSPI patch has not been applied yet")

    if args.artifacts:
        try:
            main_low, main_high = hex_range(args.artifacts / "nuttx.hex")
            stub_low, stub_high = hex_range(args.artifacts / "qspi_bootstub.hex")
        except (OSError, ValueError) as error:
            checks.check(False, "artifact HEX files parse", str(error))
        else:
            checks.check(
                0x90000000 <= main_low < main_high <= 0x98000000,
                "main artifact is entirely inside external QSPI",
                f"0x{main_low:08x}..0x{main_high:08x}",
            )
            checks.check(
                0x08000000 <= stub_low < stub_high <= 0x08020000,
                "boot stub artifact is entirely inside 128 KiB internal Flash",
                f"0x{stub_low:08x}..0x{stub_high:08x}",
            )

    return 1 if checks.failed else 0


if __name__ == "__main__":
    sys.exit(main())
