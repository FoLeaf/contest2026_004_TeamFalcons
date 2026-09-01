# Frame stats notes

## 字段（`vg_fs_summary`）

| 字段 | 含义 |
|------|------|
| `total` | 窗口内事务总数 |
| `ok` / `crc_err` / `timeout` / `echo` / `other` | 各结果计数 |
| `success_rate` / `timeout_rate` / `crc_rate` | 占比（0–100，整数百分） |
| `lat_min_ms` / `lat_max_ms` / `lat_avg_ms` | 仅 **OK** 样本；无 OK 时为 0 |

结果枚举：`VG_FS_OK` | `VG_FS_CRC` | `VG_FS_TIMEOUT` | `VG_FS_ECHO` | `VG_FS_OTHER`。

## 窗口

- 每从站一个环形缓冲，默认 **N=64**（`VG_FS_WINDOW_SIZE`，Kconfig 可关模块）。
- 仅内存，复位或重启清零。

## CRC 计数

不重算 CRC；`vgmodbus` 在 nanoMODBUS 返回 `NMBS_ERROR_CRC` 时记 `crc_err`。

## 挂钩

`modbus_collector.c`：每次 `do_read` 前后 `clock_gettime(CLOCK_MONOTONIC)`，调用 `vg_fs_record(slave, result, latency_ms)`。失败时 `latency_ms=0`。

## NSH 板测（AC2–AC3）

```text
vgstats inject <addr> <ok|crc|timeout|echo|other> [latency_ms]
vgstats dump [addr]          # 省略 addr 则列出所有从站
vgstats reset [addr]
```

无从站时：

```text
vgstats inject 1 crc
vgstats inject 1 timeout
vgstats dump 1               # crc_err/timeout 应 >0
```

有从站时：

```text
vgmodbus -a 1 -n 5
vgstats dump 1               # total/ok/lat_avg 应变化
```

`dump` / `get` 只读内存，不写 RS485。

## Host 单测

```bash
make -C app/velaguard/host_tests test
```

`test_frame_stats` 注入 ok/crc/timeout/echo 序列并断言 summary。
