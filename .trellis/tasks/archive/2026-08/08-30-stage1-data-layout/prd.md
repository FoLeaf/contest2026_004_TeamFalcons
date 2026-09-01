# /data 布局与 emmc 并入 net 预设

> 父任务：`08-30-stage1-min-product`  
> **依赖**：stage0 eMMC + config_store 已通过（`/mnt/emmc`）。  
> 依据：手册 §9；stage0 笔记「阶段 1 再迁 `/data`」。

## Goal

把 bring-up 挂载点 `/mnt/emmc` 演进为正式 **`/data/velaguard/`** 布局，并将 eMMC、config_store、frame_stats 并入 **`velaguard-net`**（或新的 stage1 主预设），作为 ai_agent 与点表持久化的唯一介质。

## Confirmed Facts

| 项 | 事实 |
|----|------|
| 块设备 | `/dev/mmcsd0`，FAT + LFN |
| 当前 config | `/mnt/emmc/velaguard/config/`（`VG_CONFIG_BASEDIR`） |
| 探针 Agent 数据 | RAMMTD + littlefs（`/data` tmp）；正式集成需 eMMC |
| 手册目标布局 | `/data/velaguard/config/`、`/data/agent/` 等 |
| 构建 | `scripts/build.sh net` 为现网主预设；`emmc` 为 stage0 隔离预设 |

## Decisions（规划默认）

| 决策 | 选择 | 理由 |
|------|------|------|
| 挂载策略 | boot 挂载 eMMC → `/mnt/emmc`；symlink 或 bind **`/data` → `/mnt/emmc/data`** 或直接把 config 迁到 `/data/velaguard` | 少动 FAT 根目录；与手册路径一致 |
| 预设合并 | 在 `velaguard-net` 上 **加** SDMMC + `VG_CONFIG_STORE` + `VG_FRAME_STATS` + `READ_TO=2000` | 一个 stage1 日常烧录入口 |
| ai_agent | 本任务 **不** 启 ai_agent Kconfig；只留目录与挂载 | 由 `stage1-ai-agent` 子任务启用 |
| 自动 mkfs | **仍不做**（延续 stage0 决策 A） | 避免误清盘 |

## Requirements

- R1 boot 后存在可写 `/data/velaguard/config/`（或等价路径），`vgcfg` 无需改命令行
- R2 `scripts/build.sh net`（或新 `stage1` TARGET）一次编译含：网络 + eMMC + config_store + frame_stats
- R3 文档：`research/data-layout-notes.md` 记录路径、迁移、回滚
- R4 stage0 板测命令（`vgmodbus`/`vgcfg`/`vgstats`）在新预设上仍通过

## Acceptance Criteria

- [x] **AC1** 新预设编译链接通过
- [x] **AC2** 板端：`mount` 见 eMMC；`/data/velaguard/config` 可写；`vgcfg commit` + 复位读回（2026-08-30 11:49–11:51 COM3）
- [x] **AC3** 同镜像：`vgmqtt`/DHCP 仍通（failover esp01→rj45；ping 10/10；`vgnet: mqtt online`）
- [x] **AC4** `vgmodbus` + MThings 仍读通（`[0]=1000 [1]=800`；`total=5 ok=5 lat_avg≈1026ms`）
- [x] **AC5** 笔记落盘

## Out of Scope

- ai_agent Kconfig / Skill / 工具
- LVGL、Modbus 自动扫描
- OTA、规则引擎

## References

- `.trellis/tasks/archive/2026-08/08-29-stage0-emmc/research/emmc-bringup-notes.md`
- `.trellis/spec/backend/vg-config-store.md`
