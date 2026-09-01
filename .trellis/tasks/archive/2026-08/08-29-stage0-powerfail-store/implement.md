# 实施计划：掉电安全配置存储

> 先用户批准本 planning，再 `task.py start`。

## Phase 0 — 准备

- [x] **0.1** 确认板上 `/mnt/emmc` 可写（emmc 已验）
- [x] **0.2** 浏览 `app/velaguard/` + host_tests
- [x] **0.3** CRC32 自实现（bit-wise）

## Phase 1 — 主机单测优先（AC1）

- [x] **1.1** `vg_config_store.c/h` 双槽逻辑
- [x] **1.2** `host_tests/test_config_store.c`
- [x] **1.3** **AC1** `make -C app/velaguard/host_tests test` OK

## Phase 2 — 板上集成（AC2–AC4）

- [x] **2.1** 启动 `vg_config_load` + `vgcfg` NSH；`CONFIG_VG_CONFIG_STORE`；emmc defconfig
- [x] **2.2** 固件已编过（含 Register: vgcfg）
- [x] **2.3** **AC2** 板端：commit + 复位读回（`OK seq=2 name=mydev`）
- [x] **2.4** **AC3** 损坏一槽恢复（damage b → `seq=3 slotA`）
- [x] **2.5** **AC4** 双坏回退默认（damage a → FACTORY）

## Phase 3 — 收尾（AC5）

- [x] **3.1** `research/powerfail-store-notes.md`
- [x] **3.2** 板端 AC1–AC5 勾完（2026-08-29）

## 回滚

- 仅增 app 层与测试；不改 SDMMC 板级

## start 前检查

- [x] eMMC 子任务 AC 已通过
- [ ] 用户批准本 planning summary
