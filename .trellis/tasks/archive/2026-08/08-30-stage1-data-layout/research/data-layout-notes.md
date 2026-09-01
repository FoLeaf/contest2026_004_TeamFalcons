# Stage1 data layout notes

> AC5 · task `08-30-stage1-data-layout` · 2026-08-30

## Verdict

**PASS** — AC1–AC5 全部通过（2026-08-30 11:49–11:51 COM3）

| AC | 结果 | 证据 |
|----|------|------|
| AC1 编译 | ✓ | `bash scripts/build.sh net --clean` |
| AC2 /data + vgcfg | ✓ | symlink `/data`；`commit stage1 seq=3`；冷复位后 `stage1-final` + `OK seq=3` |
| AC3 网络 | ✓ | 启动 `mqtt online esp01`；RJ45 就绪后 `ping 10/10` + `mqtt online rj45` |
| AC4 RS485/Modbus | ✓ | `vgrs485 tx`；`vgmodbus` `[0]=1000,[1]=800`；`total=5 ok=5` |
| AC5 笔记 | ✓ | 本文件 |

## Layout

| 路径 | 说明 |
|------|------|
| `/dev/mmcsd0` | eMMC 块设备（FAT + LFN） |
| `/mnt/emmc` | vfat 挂载点（bring-up 不变） |
| `/mnt/emmc/data/velaguard/config/` | config_store 物理目录 |
| `/mnt/emmc/data/agent/` | ai_agent 占位（下个子任务启用） |
| `/data` | symlink → `/mnt/emmc/data` |
| `/data/velaguard/config` | `CONFIG_VG_CONFIG_BASEDIR`（vgcfg 默认） |

## Build

```bash
bash scripts/build.sh net --clean    # stage1 日常入口（网络 + eMMC + vgcfg + vgstats）
bash scripts/build.sh emmc --clean   # stage0 回归（旧 BASEDIR `/mnt/emmc/velaguard/config`）
```

Preset: `stm32h750b-dk:velaguard-net`

## Migration（stage0 → stage1）

首次挂载成功后，板级 `vg_setup_data_layout()`：

1. `mkdir /mnt/emmc/data/...`
2. 若存在 `/mnt/emmc/velaguard` 且无 `/mnt/emmc/data/velaguard` → **rename** 迁移整目录（保留 `point_table_*.json` 与 seq）
3. `symlink /data -> /mnt/emmc/data`

无需手工改 FAT 根目录文件；`vgcfg` 命令不变。

## Rollback

- 软件：checkout 旧 defconfig/bringup，`build.sh emmc`
- 数据：FAT 上 `/mnt/emmc/data/` 与旧 `/mnt/emmc/velaguard/` 可并存；回滚 preset 后 vgcfg 读旧 BASEDIR

## Board AC（2026-08-30 ✓）

```text
mount → /mnt/emmc vfat；/data symlink；velaguard/config + agent 目录存在
迁移：stage0 point_table_*.json 已在 /data/velaguard/config
vgcfg probe → commit stage1 seq=3 → 冷复位 → dump OK + vg_ac.txt=stage1-final
ping 223.5.5.5 10/10（RJ45 就绪后）；vgnet mqtt online
vgmodbus 从站1 + vgstats total=5 ok=5
```

板端命令清单：`scripts/stage1_accept_nsh.txt`

## Files touched

| 位置 | 内容 |
|------|------|
| `nuttx/.../velaguard-net/defconfig` | 合并 SDMMC + VG_CONFIG + VG_FRAME_STATS |
| `nuttx/.../stm32_bringup.c` | `vg_setup_data_layout()` |
| `app/velaguard/` | BASEDIR 默认、等待 `/data` |
| `scripts/build.sh` | net 形态校验 |
| `scripts/ensure-upstream-velaguard-trees.sh` | net 上游校验 |

## Next

- `08-30-stage1-ai-agent`：启用 ai_agent + `/data/agent` 持久化
- Modbus 探查 / LVGL 可并行规划
