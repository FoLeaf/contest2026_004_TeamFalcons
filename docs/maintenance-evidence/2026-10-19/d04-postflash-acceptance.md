# 23:48 镜像 D-04 延迟复验

记录时间：2026-10-19 09:16–09:23（Asia/Shanghai）

测试对象为 [10-19 刷写与短验收](postflash-short-acceptance.md) 记录的 clean 镜像，板端版本为 `NuttX 0.0.0 2373ab4a45d Oct 18 2026 23:48:40 arm stm32h750b-dk`。测试期间没有烧录或主动重启。

每组先启动 COM7 从站并采集板端 `vgruntime dump`，约 59–65 秒后再采集一次。比较同一从站在两次快照中的累计计数差值，避免把测试前的历史计数带入本组。每组检查 14 个已确认点位的在线与故障状态；各组结束后均通过控制文件请求从站正常退出。

| 回复延迟 | 采样时间 | 请求增量 | 成功增量 | 超时增量 | CRC 增量 | echo 增量 | other 增量 | 在线点位 |
|---:|---|---:|---:|---:|---:|---:|---:|---:|
| 0 ms | 09:16:56 → 09:18:00 | 462 | 462 | 0 | 0 | 0 | 0 | 14 / 14 |
| 5 ms | 09:18:31 → 09:19:30 | 392 | 392 | 0 | 0 | 0 | 0 | 14 / 14 |
| 20 ms | 09:19:48 → 09:20:50 | 366 | 366 | 0 | 0 | 0 | 0 | 14 / 14 |
| 150 ms | 09:21:50 → 09:22:54 | 215 | 215 | 0 | 0 | 0 | 0 | 14 / 14 |

四组板端计数差均为全部成功；快照中的 14 点均为 `online=1`、`kind=none`。各组完整从站会话计数也一致：0 ms 为 694/694，5 ms 为 488/488，20 ms 为 700/700，150 ms 为 289/289；`suppressed=0`、`crc_scan=0`、`dropped=0`。从站会话计数覆盖启动到停止的时间，板端表格只使用两次板端快照的差值。

| 回复延迟 | 板端采样前 | 板端采样后 | 从站会话 |
|---:|---|---|---|
| 0 ms | [before](d04-0ms-before.log) | [after](d04-0ms-after.log) | [COM7](d04-0ms-slave.log) |
| 5 ms | [before](d04-5ms-before.log) | [after](d04-5ms-after.log) | [COM7](d04-5ms-slave.log) |
| 20 ms | [before](d04-20ms-before.log) | [after](d04-20ms-after.log) | [COM7](d04-20ms-slave.log) |
| 150 ms | [before](d04-150ms-before.log) | [after](d04-150ms-after.log) | [COM7](d04-150ms-slave.log) |

完成四组后重新启动 COM7 0 ms 正常从站。09:23:21 的板端快照显示 uptime 2342 秒、14 点全部在线且 `kind=none`，Agent 为 idle；[恢复后快照](d04-restored-0ms-baseline.log)。

## 原始记录 SHA256

| 文件 | SHA256 |
|---|---|
| [0 ms before](d04-0ms-before.log) | `95ca6c99f5b59b6c337dc285a620ab4d357fd8c797fef5bc175684c6a5fcf001` |
| [0 ms after](d04-0ms-after.log) | `2c53f7fe8c70089660dbe00b262a5cfc4161fab9e9056586928714e43a86cf40` |
| [0 ms COM7](d04-0ms-slave.log) | `acda5eee8d24deb01157b10e7d71839741e7864c3caaa9c340436ebf4df17ecd` |
| [5 ms before](d04-5ms-before.log) | `a3a0a0f42d504be7009938f137e58cbe218dde692b3244bc629a06d997754392` |
| [5 ms after](d04-5ms-after.log) | `1b91d6b017e2788ca307e2316a39f2985ff709d922ecc16f07196ac9a008a1f4` |
| [5 ms COM7](d04-5ms-slave.log) | `d011260a9bf94b3c6b878f3f1ab05b91d109cb5ee2d3f8824450242a558c3bd7` |
| [20 ms before](d04-20ms-before.log) | `db113f6f0ba1975016d91e65ae56fcbe7dc8d58a29b14e9eaf1abae2f2734b6a` |
| [20 ms after](d04-20ms-after.log) | `7fd888d189a8670d55a215563eccef46334099711eb95c234d39016455c002ca` |
| [20 ms COM7](d04-20ms-slave.log) | `23fe49b17abfa46f610c77719eae0e880b52195cb4dbeae2b2654eeda184f689` |
| [150 ms before](d04-150ms-before.log) | `6689592216344a89b401269bd94f2fb7e5266aa1cb1939591c139b16824bb13d` |
| [150 ms after](d04-150ms-after.log) | `965cfc8301be8d087832ef5d3e658e7c5e45ecf031df64fd67acb86ac49ca955` |
| [150 ms COM7](d04-150ms-slave.log) | `9c3b0e722129591ca9e24c927092c757cbe9bcf5922851632d89ba43a6b61910` |
| [恢复后快照](d04-restored-0ms-baseline.log) | `77f8f309a1830512ec02dfec1d3fe86f6e4bedba0fa986fafbad790e3ca7471c` |

结论：D-04 的 0/5/20/150 ms 回复延迟在 2026-10-19 的 23:48 镜像上复验通过。此项只覆盖 RTU 延迟与采集健康度，不代表整轮 D/E/F/G 或 24 小时长测完成。
