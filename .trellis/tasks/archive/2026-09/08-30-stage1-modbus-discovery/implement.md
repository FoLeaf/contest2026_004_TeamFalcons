# 实施计划：stage1-modbus-discovery

> 启动前：`task.py start 08-30-stage1-modbus-discovery`（用户确认后）。  
> **范围**：固定 **9600** 地址扫描，不做波特率矩阵。

## Phase A — 骨架（≈0.5d）

- [x] A1 Kconfig `VG_BUS_DISCOVER` + `VG_DISCOVER_BAUD=9600` + Makefile 挂接
- [x] A2 `vg_bus_scan`（`vg_discover_modbus.c`）：9600 下逐地址 FC03 试探 + inter-frame delay
- [x] A3 NSH `vgdiscover scan` 打印 `(addr, ok)` 表格
- [x] A4 host_test：`test_discover`（无硬件）

**Gate**：`build.sh net` OK；`vgdiscover scan -a 1-8` 板端不 crash

## Phase B — 探测与 JSON（≈1d）

- [x] B1 `vg_reg_probe_slave`：块步进 + sample 缓存
- [x] B2 `vg_point_table.c`：candidate JSON 写 `/data/velaguard/discover/`
- [x] B3 字序 MVP decode + candidates 数组
- [x] B4 `vgdiscover probe|dump|test-read`
- [x] B5 笔记 `research/modbus-discovery-notes.md`

**Gate**：AC2–AC4 板端 Modbus Slave @9600 通过（含 **32/32**）

## Phase C — 落盘与集成（≈0.5d）

- [x] C1 扩展 config：sidecar `points.json` + slot 引用字段
- [x] C2 `vgdiscover apply --confirm` → `vg_config_commit`
- [x] C3 `modbus_collector` / `vgmodbus` 对照路径已文档化
- [x] C4 RS485 锁 / 与采集互斥（最小：扫描提示 + 单主站约定）

**Gate**：AC5 `points.json`/`vgcfg dump`；host_tests 全绿

## Phase D — 验收

- [x] D1 `scripts/stage1_modbus_discovery_accept_nsh.txt` + `.ps1`（**8/8 PASS**）
- [x] D2 PRD AC 勾选；笔记更新
- [ ] D3 父任务 9/20「Modbus 自动扫描 + 点表生成」勾选（归档本任务后）

## 验证命令

```bash
# host
make -C app/velaguard/host_tests test

# board
bash scripts/build.sh net --clean && bash scripts/flash.sh
# nsh>
vgdiscover scan -a 1-8
vgdiscover probe -a 1
vgdiscover dump
vgdiscover test-read -a 1 -r 0 -c 2
vgdiscover apply --confirm
vgcfg dump
```

## 回滚

- 新 Kconfig 默认 `n`；`velaguard-net` 可后置启用
- candidate/apply 仅写 discover 目录与 inactive slot；`vgcfg damage` 恢复 factory
