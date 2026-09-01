# Stage0 验收测试（MThings 参考）

> 总线 Mock / 点表基准：**`velaguard.mthings`**  
> Windows：`C:\Users\19y\Documents\mthings\velaguard.mthings`  
> 选手仓：`config/mthings/velaguard.mthings`（已与主副本对齐）

## 进度（2026-08-30 终验 ✓）

| 节 | 状态 | 证据（2026-08-30 11:35–11:36 COM3 日志） |
|----|------|------|
| 1 eMMC | ✓ | `stage0-final` 写读；冷复位后 `cat` 仍为 `stage0-final` |
| 2 RS485 | ✓ | 此前 `vgrs485 tx/rx` + 2026-08-29 Hex |
| 3 Modbus+点表 | ✓ | MThings 从站 1/2/3：`[0]=1000,[1]=800` / `[2]=1` / PT100×4 |
| 4 帧统计 | ✓ | inject `crc=1,timeout=1`；三轮 `total=5 ok=5 lat_avg≈1022ms` |
| 5 掉电 | ✓ | `vgcfg probe`→`commit stage0ac seq=2`→复位后 `OK seq=2` |

**MThings**：COM6 **8N1**、从站仿真开、采集关；从站应答 ~1s → 板端 `READ_TO=2000ms`。

板端命令清单：`scripts/stage0_accept_nsh.txt`

## 前置

1. 固件：`bash scripts/build.sh emmc`，烧录 `velaguard-emmc`。
2. RS485：PC USB-RS485 映射 **COM6**，与板 A/B/**共地** 相连。
3. MThings：打开 `velaguard.mthings`，确认 COM6 = 9600 8N1，标题栏显示三从站 Mock 总线。
4. 需要板端主站测试时 **停止 MThings 采集**（避免双主站冲突）。

---

## 1. eMMC（Cross-AC ✓）

与 MThings 无关。

```text
ls /dev/mmcsd0
mount
echo stage0 > /mnt/emmc/vg_ac.txt
# 复位
cat /mnt/emmc/vg_ac.txt
```

---

## 2. RS485 时序

**目标**：DIR/TC 正确；应用层无 `usleep(50000)`  crutch。

### 2a 板端自测（无 Modbus）

```text
vgrs485 tx
vgrs485 rx    # 对端发字母表 → PASS
```

PC 侧：串口助手 Hex 监视 COM6，重复 `vgrs485 tx` ≥3 次，每帧 `61…7A`（26 字节）完整、无 mid-frame 乱码。

### 2b 与 MThings 的关系

- `velaguard.mthings` 提供 **从站 1/2/3 点表与 9600 参数**，不替代 LA/Hex 物理层证据。
- 物理层通过后，再用下节 Modbus 对照点表做 **链路层** 验收。

---

## 3. Modbus 采集（对照 velaguard.mthings）

点表见 `config/mthings/README.md`。板端主站（MThings **已停**）：

```text
vgmodbus -a 1 -r 0 -c 2 -n 10 -i 1
vgmodbus -a 2 -r 2 -c 1 -n 10 -i 1
vgmodbus -a 3 -r 0 -c 4 -n 10 -i 1
```

**通过判据**：

- 无从站时：超时/CRC 行为稳定，无 DIR 乱码导致的 **间歇性** 半帧。
- 有从站或 MThings 从站模拟时：返回寄存器值与 HMI 趋势一致（温湿度 / 水浸枚举 / PT100 四通道）。

可选：仅开 MThings 采集、板端不跑 `vgmodbus`，目视 HOME 页卡片/曲线刷新，确认 **COM6 侧** 从站模拟正常（与板端主站测试互斥）。

---

## 4. 帧级质量统计

### 4a 无总线（inject）

```text
vgstats inject 1 crc
vgstats inject 1 timeout
vgstats dump 1
```

`crc_err`、`timeout` 计数上升；`dump` 不写 RS485。

### 4b 有总线（与点表从站对齐）

MThings 停止后：

```text
vgstats reset 1
vgmodbus -a 1 -r 0 -c 2 -n 20 -i 1
vgstats dump 1
```

**通过判据**：`total`、`ok` 增加；`lat_avg_ms` 合理（通常 < 2000 ms 超时窗口）；失败时 `crc_err`/`timeout` 与串口现象一致。

对从站 2、3 重复 `-a 2`、`-a 3`。

---

## 5. 掉电安全存储

与 MThings 无关。见 `08-29-stage0-powerfail-store` 笔记：`vgcfg` 写读、50 次断电注入。

---

## 6. 父任务 Cross-AC 勾选

| 项 | 证据来源 |
|----|----------|
| eMMC | NSH 重启读回 |
| RS485 | Hex/LA + `vgrs485`；无应用层 50 ms sleep |
| 帧统计 | `vgstats inject` + `vgmodbus` + `dump`；点表从站 1/2/3 |
| 掉电安全 | `vgcfg` + 断电脚本 |

---

## References

- `config/mthings/velaguard.mthings`
- `config/mthings/velaguard_sensors.csv`
- `archive/.../08-29-stage0-rs485-timing/research/rs485-tc-dir-notes.md`
- `archive/.../08-29-stage0-frame-stats/research/frame-stats-notes.md`
