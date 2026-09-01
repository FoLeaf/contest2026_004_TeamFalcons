# 执行：stage1-data-layout

## Checklist

- [x] `velaguard-net/defconfig` 合并 emmc + VG_CONFIG + VG_FRAME_STATS + BASEDIR
- [x] `stm32_bringup.c`：挂载后 mkdir + symlink `/data` + stage0 路径迁移
- [x] `velaguard.c` / `vg_config_store.c`：等待 `/data` 而非硬编码 `/mnt/emmc`
- [x] `Kconfig` 默认 BASEDIR → `/data/velaguard/config`
- [x] `scripts/build.sh` net 形态校验
- [x] `scripts/ensure-upstream-velaguard-trees.sh` net 校验扩展
- [x] `research/data-layout-notes.md`
- [x] `bash scripts/build.sh net --clean` 编译通过
- [x] host `make -C app/velaguard/host_tests test`

## 板端 AC（用户自测）

```text
bash scripts/build.sh net --clean && bash scripts/flash.sh
mount
ls /data/velaguard/config
vgcfg probe && vgcfg commit stage1 && vgcfg dump
# 冷复位后 vgcfg dump 仍 OK
vgmqtt / ping / vgmodbus（同 stage0_accept_nsh.txt）
```
