# eMMC bring-up notes

> AC5 · 2026-08-29 · task `08-29-stage0-emmc`  
> 板资权威：BOUNDARY **V11** — `F:\Project\Embeded\H750B-DK\BOARD INFO\H750B-DK`

## Verdict

**PASS** — SDMMC1 eMMC + FAT + LFN；写文件 → 冷复位 → 读回一致。

| AC | 结果 |
|----|------|
| AC1 编译 | `bash scripts/build.sh emmc --clean` OK |
| AC2 设备 | `/dev/mmcsd0`（及 boot0/boot1/rpmb） |
| AC3 挂载 | 启动日志 `eMMC mounted at /mnt/emmc` |
| AC4 掉电 | `vg_emmc_probe.txt` 复位后仍为 `hello` |

## Build / flash

```bash
bash scripts/build.sh emmc --clean          # 发布
bash scripts/build.sh emmc --clean --debug  # F5
```

- Preset: `stm32h750b-dk:velaguard-emmc`（基于 net + SDMMC/FAT，不污染 net/ai-probe）
- 产物: `.debug/nuttx.hex` + `qspi_bootstub.hex`

## Hardware (V11)

| 项 | 值 |
|----|-----|
| 接口 | SDMMC1，焊装 eMMC U11，无 CD |
| 接线 | CK=PC12, CMD=PD2, D0–D3=PC8–11, D4–D5=PB8–9, D6–D7=PC6–7 |
| QSPI BK2 CS | BSP=`PG6`（勿采 unofficial PC11 冲突臆测） |
| 时钟沿 | `board.h` rising（对齐 ST BSP） |
| 证据 | `bsp/stm32h750b_discovery_mmc.c` MSP；`research/um-emmc-pinout.md` |

## Software path

1. `stm32_sdio_initialize()` → `sdio_mediachange(true)` → `/dev/mmcsd0`
2. Bring-up：`nx_mount("/dev/mmcsd0", "/mnt/emmc", "vfat", …)`
3. 无 FAT 时：`WARNING: … failed: -22` → NSH `mkfatfs /dev/mmcsd0` → 再启自动挂载

挂载点 bring-up 用 `/mnt/emmc`；正式 `/data/...` 留给阶段 1 / powerfail-store。

## Kconfig（要点）

```text
CONFIG_STM32H7_SDMMC1=y
CONFIG_MMCSD=y
CONFIG_MMCSD_SDIO=y
CONFIG_MMCSD_MMCSUPPORT=y
# CONFIG_MMCSD_HAVE_CARDDETECT is not set
# CONFIG_STM32H7_SDMMC_IDMA is not set   # MM_REGIONS=6
CONFIG_FS_FAT=y
CONFIG_FAT_LFN=y
CONFIG_SDMMC1_SDIO_PULLUP=y
CONFIG_FSUTILS_MKFATFS=y
```

## First-time board setup

```text
mkfatfs /dev/mmcsd0
# 复位后应见: eMMC mounted at /mnt/emmc
echo hello > /mnt/emmc/vg_emmc_probe.txt
# 复位
cat /mnt/emmc/vg_emmc_probe.txt
```

未开 LFN 时仅 8.3（如 `HELLO.TXT`）；`vg_emmc_probe.txt` 需要 `CONFIG_FAT_LFN=y`。

## Known limits / pitfalls

1. **总线宽度**：NuttX H7 `CLOCK_MMC_TRANSFER` 固定 WIDBUS_D1；默认路径为 **1-bit MMC**。D4–D7 已配 AF，真 4/8-bit 需后续改驱动。小文件配置场景可接受。
2. **IDMA**：关；编译期 Non-DMA RX overrun warning 预期。
3. **首次挂载 -22**：无文件系统，不是硬件坏。
4. **LFN**：见 NuttX NOTICE（Microsoft 专利免责）。
5. **公共仓**：板级改动在 nuttx feature 分支；选手仓改 `scripts/build.sh` / ensure。

## Files touched

| 位置 | 内容 |
|------|------|
| `nuttx/.../stm32h750b-dk/include/board.h` | SDMMC1 GPIO + rising |
| `.../src/stm32_sdmmc.c` | 无 CD 初始化 + D4–D7 |
| `.../src/stm32_bringup.c` | init + mount `/mnt/emmc` |
| `.../configs/velaguard-emmc/defconfig` | 预设 |
| `contest.../scripts/build.sh` | TARGET `emmc` |
| `contest.../scripts/ensure-upstream-velaguard-trees.sh` | 校验 |

## Next

- 子任务 `08-29-stage0-powerfail-store`（双槽 CRC/seq）可启动规划/实现
- RS485 时序可与本任务并行
- 阶段 1：挂载布局迁到 `/data/velaguard/...`
