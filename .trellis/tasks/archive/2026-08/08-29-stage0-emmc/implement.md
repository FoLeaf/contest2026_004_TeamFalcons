# 实施计划：eMMC bring-up

> **先用户批准本计划，再 `task.py start`（允许空 context 或先 curated jsonl）。**

## Phase 0 — 准备

- [x] **0.1** 对照本地官方包：`bsp/stm32h750b_discovery_mmc.c` MSP + research 复核（V11）
- [x] **0.2** 通读 `openh743i`/`linum` `stm32_sdmmc.c` + ST BSP；知悉 H7 驱动默认仅配 D0–D3、MMC 常走 1-bit
- [ ] **0.3** 确认 `velaguard-net` 仍可 `bash scripts/build.sh net`（回归基线；可与 emmc 构建后交叉）

## Phase 1 — 板级驱动与配置（AC1）

- [x] **1.1** `board.h` 增加 SDMMC1 8-bit GPIO + rising edge
- [x] **1.2** `stm32h750b-dk.h` 增加 `HAVE_SDIO` 等宏
- [x] **1.3** 新增 `stm32_sdmmc.c`（无 CD，强制 media present）
- [x] **1.4** Makefile/CMake 条件编译；bringup 初始化 + mount `/mnt/emmc`
- [x] **1.5** 新增 `configs/velaguard-emmc/defconfig`
- [x] **1.6** `scripts/build.sh` + ensure 脚本支持 `emmc` TARGET
- [x] **1.7** **AC1**：`bash scripts/build.sh emmc --clean` 通过（2026-08-29；增量修 GPIO include 后 `build.sh emmc` 链接成功）

## Phase 2 — 板端（AC2–AC4）

- [x] **2.1** 烧录；`/dev/mmcsd0` 存在（AC2）
- [x] **2.2** NSH 手动 `mkfatfs`（决策 A）
- [x] **2.3** 启动日志 `eMMC mounted at /mnt/emmc`（AC3）
- [x] **2.4** `vg_emmc_probe.txt`（LFN）冷启仍在（AC4，2026-08-29）

## Phase 3 — 收尾（AC5）

- [x] **3.1** `research/emmc-bringup-notes.md` 定稿
- [x] **3.2** 摘要进 `docs/velaguard-bringup-known-issues.md` §7
- [x] **3.3** 一致性核对（defconfig / bringup / build TARGET）；父任务可勾 eMMC；下一子任务 powerfail-store

> 提交：请人工 `git add` + commit（选手仓 + 按需 nuttx 分支）。未自动 commit。

## 回滚

- 仅影响 `velaguard-emmc` 与新增板级文件；net/ai-probe 预设不动

## start 前检查

- [x] 用户选定首次无 FAT 策略 = A（手动 mkfatfs）
- [x] 用户批准本 planning summary / 硬件复审后 `task.py start`
