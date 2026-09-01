# eMMC bring-up（SDMMC1 + FAT）

> 父任务：`08-29-stage0-foundation`  
> 依据：推进方案 §5.1 eMMC；手册 §3.1 / §9。

## Goal

在 STM32H750B-DK 上启用板载 eMMC：板级 SDMMC 初始化 + FAT 挂载，**重启后文件仍在**。

## Confirmed Facts（本地官方板资 + 交叉审阅）

> 权威：`BOUNDARY.md` **V11** — `F:\Project\Embeded\H750B-DK\BOARD INFO\H750B-DK`（原理图 > `bsp/` > UM）。  
> 详见 `research/um-emmc-pinout.md`（已对照 BSP `stm32h750b_discovery_mmc.c`）。

| 项 | 事实 |
|----|------|
| 实例 | eMMC（U11）接 **SDMMC1** |
| 总线 | **8-bit**（BSP `SDMMC_BUS_WIDE_8B`） |
| 引脚 | D0–D3=`PC8–11`，D4–D5=`PB8–9`，D6–D7=`PC6–7`，CK=`PC12`，CMD=`PD2`（BSP MSP AF12） |
| PC11 / QSPI | unofficial 笔记曾标与 QSPI_BK2_NCS 可选冲突；**BSP 实际 BK2 CS=`PG6`**，与 eMMC 并存可接受 |
| 卡检测 | 焊装，无 CD |
| 容量 | ST 常写 4 Gbyte；项目手册写 8 GB — bring-up 不依赖 |
| 板级驱动 | NuttX `stm32h750b-dk` **无** `stm32_sdmmc.c`；ST BSP 有 `stm32h750b_discovery_mmc.*` 可作参考 |
| 时钟宏 | `board.h` 已有 `STM32_SDMMC_*_CLKDIV` |
| IDMA | 现有 defconfig `MM_REGIONS=6` → bring-up 默认 **关 IDMA** |
| 存储现状 | `velaguard-net` 无持久存储；ai-probe 用 RAMMTD+littlefs |
| 文件系统 | 块设备 → **FAT/vfat** |
| 边界 | 公共仓板级改动最终走 fork/PR |

## Requirements

- R1 板级 pinmux：`board.h` 定义 SDMMC1 8-bit GPIO
- R2 板级 `stm32_sdmmc.c`：`sdio_initialize` → `mmcsd_slotinitialize`，无卡检测，上电 `sdio_mediachange(..., true)`
- R3 `stm32_bringup.c` 调用初始化，并挂载 FAT（见下方决策）
- R4 专用或扩展 defconfig：`STM32H7_SDMMC1` + `MMCSD` + `MMCSD_MMCSUPPORT` + `FS_FAT`，关 IDMA / 关 CARDDETECT
- R5 `scripts/build.sh` 可构建该配置（新 TARGET 或文档化入口）
- R6 板端验收：写测试文件 → 冷重启 → 读回一致

## Decisions

| 决策 | 选择 | 理由 |
|------|------|------|
| 首次无 FAT | **A：挂载失败只打日志，NSH 手动 `mkfatfs`** | 避免误清 Cube/旧数据；开发验收可接受多一步 |
| 挂载点（bring-up） | `/mnt/emmc` | 与现有 `/mnt/lfs` 并列；`/data` 留给阶段 1 正式布局 |
| 总线宽度 | 8-bit（硬件接线） | ST UM 确认；失败再退 4-bit 对照 |
| IDMA | 关闭 | 兼容 `MM_REGIONS=6` |

## Acceptance Criteria

- [x] **AC1** 带 SDMMC1+eMMC+FAT 的镜像编译链接成功
- [x] **AC2** 启动日志显示 mmcsd/eMMC 初始化成功；`/dev/mmcsd0`（或约定设备节点）存在
- [x] **AC3** FAT 已挂载到约定挂载点
- [x] **AC4** `echo hello > <mount>/vg_emmc_probe.txt` → 断电/复位 → `cat` 仍为 `hello`（含 LFN）
- [x] **AC5** 简短 bring-up 笔记：`research/emmc-bringup-notes.md` + `docs/velaguard-bringup-known-issues.md` §7

## Out of Scope

- `/data/agent` 与 ai_agent 正式迁移（阶段 1）
- 双槽掉电安全配置（子任务 `stage0-powerfail-store`）
- OTA staging 镜像
- microSD（项目明确不用）
- 上游 PR 合入（可先本地 feature 分支；PR 另开或并入本任务收尾）

## Risks

| 风险 | 应对 |
|------|------|
| 首次无文件系统 / Cube 残留分区 | 开发期允许 NSH `mkfatfs` 一次；是否自动 mkfs 见规划待决 |
| 8-bit 调不通 | 临时退 4-bit（仅 D0–D3）对比 |
| IDMA + 多 heap region | 关 `CONFIG_STM32H7_SDMMC_IDMA` |
| 与 UART7/ETH 等引脚冲突 | 对照 UM 引脚表；PD2 为 CMD，确认无冲突外设占用 |

## References

- ST Discovery kit UM（H745/H750）：§6.9 eMMC → SDMMC1，8-bit 引脚表
- `nuttx/boards/arm/stm32h7/openh743i/src/stm32_sdmmc.c`
- `nuttx/boards/arm/stm32h7/linum-stm32h753bi/include/board.h`（SDMMC1 pinmux 参考）
