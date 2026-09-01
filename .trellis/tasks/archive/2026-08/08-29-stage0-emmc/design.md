# 设计：eMMC bring-up

## 1. 边界

| 层 | 改动位置 | 说明 |
|----|----------|------|
| 公共仓 nuttx 板级 | `boards/.../stm32h750b-dk/` | pinmux、`stm32_sdmmc.c`、bringup、defconfig |
| 选手仓 | `scripts/build.sh` + ensure/patch | TARGET / 上游树校验；不把业务 JSON 写进公共仓 |
| 不做 | auto-mkfs、`/data` 布局、掉电双槽、ai_agent 迁移 | 见 PRD Out of Scope + 决策 A |

## 2. 硬件契约（冻结，V11 已复核）

- SDMMC1，8-bit，无 CD
- CMD=PD2，CK=PC12，D0–D3=PC8–11，D4–D5=PB8–9，D6–D7=PC6–7
- 证据：本地官方包 `bsp/stm32h750b_discovery_mmc.c` MSP + schematic；`research/um-emmc-pinout.md`
- QSPI BK2 CS 以 BSP 为 **PG6**，不与 PC11/eMMC D3 冲突（勿采 unofficial 可选焊桥臆测为默认）

## 3. 软件数据流

```text
boot
 → stm32_sdio_initialize()
     → sdio_initialize(slot)
     → mmcsd_slotinitialize(minor, sdio)
     → sdio_mediachange(sdio, true)   // 焊装 eMMC
 → nx_mount("/dev/mmcsd0", "/mnt/emmc", "vfat", 0, NULL)
     → 成功：syslog OK
     → 失败：syslog 提示手动 mkfatfs，不自动格式化
```

## 4. Kconfig（最小集）

```text
CONFIG_STM32H7_SDMMC1=y
CONFIG_MMCSD=y
CONFIG_MMCSD_SDIO=y
CONFIG_MMCSD_MMCSUPPORT=y
# CONFIG_MMCSD_HAVE_CARDDETECT is not set
# CONFIG_STM32H7_SDMMC_IDMA is not set
CONFIG_FS_FAT=y
```

可选：`velaguard-emmc` 新预设（基于 net 加法）或 `ai-probe`/`net` 叠加——推荐 **独立 `velaguard-emmc` 预设**，避免污染 net/ai-probe 验收基线；通过后阶段 1 再合并进正式预设。

## 5. 板级文件计划

| 文件 | 动作 |
|------|------|
| `include/board.h` | 增加 `GPIO_SDMMC1_*`（8-bit） |
| `src/stm32h750b-dk.h` | `HAVE_SDIO`、`SDIO_SLOTNO`、`SDIO_MINOR`；无 NCD |
| `src/stm32_sdmmc.c` | 新建（openh743i 模板，去掉 CD 中断） |
| `src/stm32_bringup.c` | 调初始化 + 尝试 mount |
| `src/Makefile` / `CMakeLists.txt` | `CONFIG_STM32H7_SDMMC` 条件编译 |
| `configs/velaguard-emmc/defconfig` | 新建 |

选手仓：`scripts/build.sh` 增加 `emmc` TARGET；`ensure-upstream-velaguard-trees.sh` 校验 defconfig 存在。

## 6. 兼容与回滚

- 不改 `velaguard-net` / `ai-probe` 默认行为
- 回滚：删/不启用 `velaguard-emmc` 预设；板级文件可留在 feature 分支
- IDMA 关闭后吞吐足够 bring-up；日后若要开 IDMA，需单独评估 heap regions

## 7. 验收路径

1. `bash scripts/build.sh emmc --clean`
2. 烧录 → NSH：`ls /dev/mmcsd0`（或实际节点名）
3. 若 mount 失败：`mkfatfs /dev/mmcsd0` 后 remount / 重启再挂
4. 写 `/mnt/emmc/vg_emmc_probe.txt` → 复位 → `cat` 一致

## 8. 与后续子任务接口

- `stage0-powerfail-store`：在 `/mnt/emmc`（或日后 `/data/velaguard/config`）写双槽文件
- 阶段 1：可将 `/mnt/emmc` 上建 `/data/...` 或改挂载点；本任务不锁死最终路径
