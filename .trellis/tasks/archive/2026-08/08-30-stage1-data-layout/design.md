# 设计：/data 布局 + net 预设合并

## 边界

| 层 | 改动 | 说明 |
|----|------|------|
| nuttx `velaguard-net/defconfig` | 合并 emmc Kconfig | 日常烧录入口 = net |
| nuttx `stm32_bringup.c` | eMMC 挂载后建 `/data` 布局 | 物理在 FAT `/mnt/emmc/data/...` |
| 选手仓 `app/velaguard/` | 默认路径、等待挂载点 | `CONFIG_VG_CONFIG_BASEDIR` → `/data/velaguard/config` |
| 选手仓 `scripts/build.sh` | net 形态校验 | 含 SDMMC + config_store + frame_stats |
| 不做 | ai_agent Kconfig、自动 mkfs、Modbus 探查 | 留给后续子任务 |

## 挂载策略（决策）

```text
/dev/mmcsd0  →  /mnt/emmc  (vfat, 不变)
/mnt/emmc/data/velaguard/config/   ← 物理目录
/mnt/emmc/data/agent/              ← 占位（ai_agent 子任务）
/data  →  symlink → /mnt/emmc/data
```

- `CONFIG_VG_CONFIG_BASEDIR="/data/velaguard/config"`
- Stage0 旧路径 `/mnt/emmc/velaguard/`：若存在且新路径无槽文件，**rename** 整棵 `velaguard` 到 `data/velaguard`（一次性迁移）

## 兼容性

- `velaguard-emmc` 预设保留（stage0 回归）；net 成为 stage1 主入口
- `vgcfg`/`vgmodbus`/`vgstats` 命令行不变
- 网络栈（DHCP/MQTT/failover）defconfig 不动

## 回滚

- `git checkout` nuttx defconfig + bringup
- 板端仍可用 `bash scripts/build.sh emmc` 回到 stage0 布局
