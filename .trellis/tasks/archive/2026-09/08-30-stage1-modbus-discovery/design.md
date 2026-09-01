# 设计：Modbus 总线自动探查（stage1 MVP）

> 对应 `prd.md` R1–R7。LVGL 确认流见 `stage1-lvgl-hmi`。  
> **决策**：阶段 1 **不扫波特率**，UART 固定 **9600**（与 mock / 现场默认一致）。

## 1. 模块边界

```text
app/velaguard/
  vg_bus_scan.c/h       固定 9600 下的从站地址扫描
  vg_reg_probe.c/h      块探测 + 字序/类型候选项
  vg_point_table.c/h    候选点表 JSON 读写、apply → vg_config
  vgdiscover.c          NSH 入口（薄封装）
  modbus_port_openvela  复用（与 vgmodbus / vg_modbus_read 一致）
  vg_config_store       apply 时 bump seq、写 inactive slot
```

**不新增** ai_agent 工具；扫描全程本地 C，断网可用。

## 2. 扫描状态机（R1）

| 阶段 | 行为 |
|------|------|
| IDLE | 打开 `/dev/rs485`；`termios` 设为 **9600** 8N1；可选暂停采集 |
| PROBE_ADDR | FC03 `@addr=0 qty=1`（或 FC04）；timeout 500ms；CRC/exception 判定 |
| INTER_FRAME | `tcdrain` + 3.5 char @ 9600 |
| NEXT | 地址++（min..max） |
| DONE | 输出 `vg_scan_result_t[]`（仅 addr + alive） |

Kconfig：

- `VG_DISCOVER_ADDR_MIN/MAX`（默认 1–32，板测可缩到 1–8 加速）
- `VG_DISCOVER_BAUD`（int，**默认 9600**；不提供运行时波特率列表）
- `VG_DISCOVER_INTER_MS`（默认 5）

## 3. 块探测（R2）

对每个存活从站（波特率恒 9600）：

1. 从站地址 + 已打开的 9600 口
2. Holding FC03：start=0, step=16, qty=16，直到连续 N 次 exception 4/2 或到达 `VG_DISCOVER_REG_MAX`（如 120）
3. 合并相邻成功块为 `vg_reg_block_t { fc, start, count, sample[4] }`
4. Input FC04：同样逻辑（可选 `-t` 只探 holding）

## 4. 字序/类型 MVP（R3）

对每个 block：

| 寄存器数 | 候选项 |
|----------|--------|
| 1 | int16 / uint16 × scale {1, 0.1, 0.01} |
| 2 | float32 × word_order {ABCD,BADC,CDAB,DCBA} |
| ≥4 | 按 2 寄存器切分重复上述 |

启发式打分（非唯一解）：

- float：值在物理合理区间（-100..200 温度，0..100 湿度）+1
- int16×0.1：同上
- 输出 top-2 候选写入 JSON `"candidates":[]`

## 5. 点表 JSON schema v1（R4）

路径：`/data/velaguard/discover/point_table_candidate.json`（未确认前不写 config slot）

```json
{
  "schema_version": 1,
  "generated_at": "2026-08-30T12:00:00Z",
  "bus": { "device": "/dev/rs485", "baud": 9600 },
  "slaves": [{
    "addr": 1, "baud": 9600,
    "points": [{
      "tag": "temp_1", "fc": 3, "reg": 0, "qty": 1,
      "dtype": "int16", "scale": 0.1, "unit": "C",
      "candidates": []
    }]
  }]
}
```

`apply`：校验 schema → 转换为 sidecar `points.json` + slot 引用 → `vg_config_commit()`。**必须** `--confirm` 才写盘。

## 6. NSH 接口（R5）

```text
vgdiscover scan [-a min-max]          # 固定 9600，无 -b
vgdiscover probe -a <addr> [-t 3|4|both]
vgdiscover dump [-o path]
vgdiscover test-read -a <addr> -r <reg> [-c qty]
vgdiscover apply [--confirm]          # 无 confirm 仅 dry-run 摘要
```

**HMI（`stage1-lvgl-hmi`）**：探查页提供「启用总线扫描」开关，**默认关闭**；仅开启后调用上述流程。NSH 不受开关限制。

## 7. 与现有代码关系

| 现有 | 用法 |
|------|------|
| `modbus_port_openvela.c` | RTU transport + RS485 DIR @ 9600 |
| `vg_frame_stats` | 扫描/probe 可选 hook（timeout/CRC 计数） |
| `vg_config_store` | apply 落盘；sidecar `points.json` |
| `config/mthings/velaguard_sensors.csv` | 板测 golden：addr=1 @9600 reg0/1 温湿度 |

**Sidecar 决策（默认）**：点表正文放 `/data/velaguard/config/points.json`，slot JSON 只增 `points_file` + `points_crc32` 字段。

## 8. 风险与裁剪

| 风险 | 应对 |
|------|------|
| 地址 1–247 扫描偏慢 | 默认 1–32；板测 `-a 1-8` |
| 从站非 9600 扫不到 | 文档说明阶段 1 限制；手工改 Kconfig 或后续阶段 2 |
| 字序误判 | 候选列表 + test-read；不自动 apply |
| 与采集争用 RS485 | scan 前警告；可选 RS485 锁 |
| FAT 大 JSON | candidate 与 points 分文件 |

## 9. 测试策略

- **Host**：JSON round-trip、word_order decode、scan 状态机 table-driven
- **Board**：MThings mock @9600 addr1；对照 CSV
- **Negative**：无 ACK 地址跳过；exception 不崩溃
