# 实施计划：帧级质量统计（MVP B）

> 用户批准本 planning summary 后，再 `task.py start`。

## Phase 0

- [ ] **0.1** 浏览 `modbus_collector.c` 返回路径与错误码
- [ ] **0.2** host_tests Makefile 模式对齐 `test_config_store`

## Phase 1 — 核 + AC1

- [ ] **1.1** `vg_frame_stats.c/h`
- [ ] **1.2** `test_frame_stats.c`：注入 ok/crc/timeout/echo，断言 summary
- [ ] **1.3** **AC1** `make -C app/velaguard/host_tests test` 通过

## Phase 2 — 板集成

- [ ] **2.1** Kconfig + Makefile：`VG_FRAME_STATS`、`vgstats`
- [ ] **2.2** `vgmodbus` 挂钩 `vg_fs_record` + 计时
- [ ] **2.3** NSH：`vgstats dump [addr]` / `inject` / `reset`
- [ ] **2.4** **AC2–AC3** 板测（无从站时用 inject）

## Phase 3 — AC4

- [ ] **3.1** `research/frame-stats-notes.md`
- [ ] **3.2** 更新父任务 cross-AC；trellis-check / commit

## 验证

```bash
make -C app/velaguard/host_tests test
bash scripts/build.sh emmc   # 或 net
# 板端
vgstats inject 1 crc
vgstats inject 1 timeout
vgstats dump 1
vgmodbus -a 1 -n 5        # 有从站时
vgstats dump 1
```

## 回滚

关掉 `VG_FRAME_STATS`；vgmodbus 恢复无挂钩。
