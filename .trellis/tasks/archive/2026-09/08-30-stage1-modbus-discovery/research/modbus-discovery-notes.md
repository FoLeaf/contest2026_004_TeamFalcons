# stage1-modbus-discovery 笔记

> 2026-08-30 · NSH MVP + 板测通过（含 **32/32** @ Modbus Slave）

## 交付

| 项 | 路径 |
|----|------|
| NSH | `vgdiscover scan|probe|dump|test-read|apply` |
| 扫描 | `vg_discover_modbus.c` — 固定 **9600**；reg **0→2→1** × qty **1→2**；异常响应也算存活；漏扫二次重试 |
| 探测 | 块步进 FC03/FC04，qty 16→2→1 回退；state → `/data/velaguard/discover/discover_state.bin` |
| 点表 | `vg_point_table.c` — infer + JSON + apply → `points.json` |
| Kconfig | `VG_BUS_DISCOVER`（velaguard-net 已启用） |
| 验收脚本 | `scripts/stage1_modbus_discovery_accept.ps1` |
| Host test | `host_tests/test_discover.c` |
| Mock 从站 | **推荐 Modbus Slave**（`config/modbus-slave/`）；MThings 全扫易闪退 |

## 根因（2026-08-30 跑飞）

`vgdiscover` NSH 任务 **STACKSIZE=4096**，但 `main` 栈上 ~4.7KB 的 `vg_discover_summary`，`state_save/load` 再叠 ~4.7KB → **栈溢出**，板子跑飞、COM3 无响应。

**修复**：状态改静态 `g_sum`/`g_sf`；`vgdiscover` STACKSIZE=8192。

## 板测（2026-08-30 晚 · Modbus Slave）

前置：`scripts/build_velaguard_mbslave.ps1 -OpenConnection -KeepOpenSeconds 900`（COM6 @9600 8N1），固件 `velaguard-net`。

| 项 | 结果 |
|----|------|
| `build.sh net` | **PASS**（flash ≈555 KB） |
| `host_tests`（含 test_discover） | **PASS** |
| `stage1_modbus_discovery_accept.ps1` | **8/8 PASS** |
| `vgdiscover scan -a 1-32` | **32/32**（~8s） |
| 早前 `vgscan`（lvgl 固件） | **32/32**（~10s） |

### Mock 选型

| | MThings | Modbus Slave |
|---|---------|--------------|
| 32 站密集扫描 | 易闪退 / 争用 | 被动应答，稳定 |
| 点表权威 | `velaguard.mthings` | 从 mthings 导出 `velaguard_slaves.csv` |

详见 `config/modbus-slave/README.md`。

### 历史漏扫（已解决）

| 原因 | 处置 |
|------|------|
| reg0-only 探活 | 探针改为 reg **0 / 2 / 1** |
| scan qty=2 打崩 MThings | qty **1 优先**；改用 Modbus Slave |
| 偶发漏扫 | 帧间隔 50ms + 二次重试 |

## UI 口径

- LVGL 总线扫描：**开关默认关闭**，用户显式开启后才 scan
- NSH `vgdiscover` 不受开关限制（bring-up）
- 详见 `VelaGuard_项目手册.md` §5.1、§6.6；`stage1-lvgl-hmi/prd.md` R4

## 板测命令

```text
# PC: Modbus Slave COM6 @9600 8N1（勿开 MThings）
vgdiscover scan -a 1-32
vgdiscover probe -a 1
vgdiscover dump
vgdiscover test-read -a 1 -r 0 -c 2
vgdiscover apply --confirm
```

## 已知限制

- 无波特率矩阵；非 9600 从站需后续扩展
- 类型推断为 int16×0.1 启发式，需 test-read + 人工确认
- probe 每次覆盖 blocks（单站 MVP）
- Mbslave COM `OpenConnection` 可能返回 0 仍可用；自动化脚本以实读为准

## 镜像

flash ≈ **555 KB**（net）
