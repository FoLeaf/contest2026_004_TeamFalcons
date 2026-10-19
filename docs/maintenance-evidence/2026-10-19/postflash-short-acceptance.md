# 2026-10-19 clean 镜像刷写与存储短验收

## 构建与镜像身份

- 构建入口为 `bash scripts/build.sh --clean`，产物日志为 `.debug/resumed-clean-build.log`，构建来源清单为 `.debug/resumed-build-source-manifest.json`。
- 构建记录起始时间为 2026-10-18 23:48:00 +08:00，最终产物时间为 23:48:45；构建日志到达 `staged`，记录 6 条 warning、0 条 error，SRAM 为 294,092 / 524,288 B（56.09%）。
- 构建日志 SHA256：`b482a7bf220f1bd105087c3c04cbce4c994a19e6d671c1bcc96b014992f754db`；来源清单 SHA256：`e822568220f9aecbea0f565d8139e2133873a53f1cdef360b3eb48ab8bbb750f`。
- 构建提交为 `305fc876bc382b50a0d44b6640a72b6f5e2bdbb5`。当前集成分支为 `ff95b81c36d7`；构建后产品源码没有变化，新增差异仅为主机测试 `app/velaguard/host_tests/test_mqtt_cred.c` 与文档。
- openvela 公共树：NuttX `2373ab4a45ddfebe67706110db460b27f3a8d127`，apps `0ea954b79740756318ce1f0b13271dd94235fe2a`，packages/ai_agent `7dc0a8816292cd58675863e9a1cde8b1cd96a535`，MQTT-C `4dbb7f321ceeed47822171787e0742df2ddb4959`。NuttX、ai_agent、MQTT-C clean；apps 保留原有三个未跟踪 NIST STS 目录，未纳入改动。
- 预设 `scripts/configs/velaguard-lvgl.defconfig` SHA256 为 `d02be7adf022e9de81bd9a1eb35b44347be21ee5e5d720e60da9943b3a2a4ccb`；生成的 `nuttx/.config` SHA256 为 `c4150e92cb327fd96b210f6db853f1130d21d6c6a349fdae98522079e90ac646`。

| 产物 | 大小 | SHA256 |
|---|---:|---|
| `.debug/nuttx.hex` | 3,560,427 B | `0ed9b23848def15096653f173929358f4aa8271873c09b80123da3dd3e0e65de` |
| `.debug/nuttx.bin` | 1,265,792 B | `259657f6323a8dd188c17132817d7fad96fb93622af7f6d3eab33d28f1f50a08` |
| `.debug/nuttx.elf` | 1,651,304 B | `b6d4cc425240618aa31724f8f83f8eb9291e3a19d17a885805c17b6e80824f44` |
| `.debug/qspi_bootstub.hex` | 2,076 B | `877aa3cf6072d66582b33eac555ded7cd5603bacd5dc794915fbcba425f6119e` |
| `.debug/qspi_bootstub.bin` | 720 B | `9e6ddfa058915cdb3fec18ff8f862a1ca7dfe2b6a31134d1bc31df037fc2026d` |
| `.debug/qspi_bootstub.elf` | 5,776 B | `25d97caed149965f29750e458f96cbb063b15af3182d8e3b13adbfe5464993c5` |

## 刷写与板端检查

- `scripts/flash.ps1 -ValidateOnly` 通过。主镜像地址范围为 `0x90000000..0x90135080`，内部 boot stub 地址范围为 `0x08000000..0x080002d0`。
- `scripts/flash.ps1` 返回码为 0；外部 QSPI 主镜像与内部 boot stub 均输出 `Download verified successfully`，随后复位目标。刷写原始日志 SHA256：`362480a2eb38c17bb7338029fcace57ed6e3a82027dc9eb9d27923378d693fb9`。
- COM3 返回板端身份 `NuttX 0.0.0 2373ab4a45d Oct 18 2026 23:48:40 arm stm32h750b-dk`，与本次 clean 构建产物时间一致。
- 在本次 23:48:40 镜像上复验 D-02：仅在 `/data/velaguard/discover_bad/` 新建唯一 `d02probe_20261019_0907_{a,b}.txt`，将 A 覆盖改名到 B，`cat` 返回 `D02-A`，`ps` 显示 PID 4 栈仍为 `0008168`，`vgruntime` 与后续命令均返回。随后仅删除本次创建的 B 文件；前后目录列表相同，原有 `point_table_candidate.json`、`discover_state.bin` 等文件未改动，未见 HardFault 或重启。复验日志 SHA256：`1cadc1d47820af29fe3f5aa37b91595a89519c106a43024189c35a37364cccdd`；清理后目录核对 SHA256：`0b9700eede1910ef6b2396f2cbfd7639375b247fde3e5d54b5a32f56f29dc8c7`。
- 刷写后的板端基线日志 SHA256：`30634fc874739e079d2042a5c099079f91f16916468d5009e52426f0c798fe71`；含在线状态检查的稳定快照 SHA256：`aae31c03dd98aefa798e3de11e58ba065b7eca3c386c9f6d92c50d26693124cc`。
- COM7 从站配置为 9600 8N1、0 ms 回复。08:45:32 至 08:52:32 记录 2,965 次请求、2,965 次应答、`crc_scan=0`，没有静默或丢帧。COM7 日志 SHA256：`51779d41dd2c24d8c54218a418379b153c0b5a41953be88670a70148d4d379a9`。
- 08:51:49 板端运行快照为 `uptime_s=451`，14 个已确认点全部 `online=1` 且 `kind=none`；`vgpoint list` 显示 `table=committed n=14`。`vgagent status` 为 idle，LLM 凭证状态为 ready。此项不代表 LLM 调用或 MQTT 通信通过。
- 从板端 `vgpoint list` 抽取的 14 行通过 `python3 scripts/alarm_scenarios/gen_alarm_scenarios.py --list-dump .debug/vgpoint-list-20261019.txt` 核对；结果为与 `vgpoint_scene_room.json` 一致，25 个工况、83 个步骤。点表记录 `temp.name=温度验收`、warn=28、crit=35；这证明配置内容，不能代替屏幕目视。点表快照 SHA256：`41b257c4e8a82a846b0826ab767929f402f6d5ca3186f3c58db76b27363f00ef`。
- 08:57:08 检查到 14 点仍在线、均为 `kind=none`，`/data/velaguard/pending_alarm.txt` 不存在；读取期间看到的是重启后最初离线告警的恢复事件。该快照日志 SHA256：`b417b304459ce47175a3989cc079cd18fec9564b023ca79be387cb21baa1d24b`。
- `vgstoretest 10` 返回 `OK rounds=10 bytes=5120 elapsed_ms=24910`。此探测仅写 `/data/velaguard/diagnostics`；没有注入存储故障，不能代替失败恢复验收。日志 SHA256：`4b18fe6ceadf536076c370a9f7818b2b5b8ad2e59c087dd6d637c77b1990e242`。
- 长测单轮参数 `vgstoretest 50` 也返回 `OK rounds=50 bytes=25600 elapsed_ms=121510`，低于正式长测配置的 240 秒命令上限；日志 SHA256：`818da8790411a763b3eb4fcfe86a4f8d667f3359bfa6d57edddac4223c20ee77`。这仍是正常写入探测，不是失败注入或恢复测试。
- 50 轮探测后 09:05:16 再查，板端 `uptime_s=1258` 连续递增，14 点全在线且 `kind=none`，Agent idle、LLM 凭证 ready，`pending_alarm.txt` 仍不存在。累计 `boot_timeout=73` 与 08:51 快照相同，COM7 从站运行期间没有新增超时；复查日志 SHA256：`745190e992431d5260ad0951aa130d2f9725a27cc48b68b46b7069a7fb55fae3`。

## D-04 延迟复验

- 2026-10-19 09:16–09:23 在同一 23:48 镜像上完成 0/5/20/150 ms 四档复验，各组按板端累计计数差值分别为 462/392/366/215 次请求全部成功，错误增量均为 0，14 个已确认点在线。之后恢复 COM7 0 ms 正常工况。原始日志及哈希见 [D-04 复验](d04-postflash-acceptance.md)。

## 凭证扫描与待验收项

- 本次 bin/elf 扫描没有输出凭证值：编译期 MQTT 用户名和密码键名均为 0 命中；运行时凭证配置开关已启用。静态 WiFi 密码仍以可打印字符串出现在 bin、elf 各 1 次（合计 2 次），沿用已知待处理项；扫描记录只保存命中数。扫描日志 SHA256：`0df67ad2eef4f190cbfbfebc20f6298fedf500dffab6d7b08b45c92f5202df71`。
- 温度基线当前为 24°C。屏幕名称和刷新尚未目视验收；刷新项需由真人观察时将模拟值从 24°C 改到 30°C，再恢复 24°C，并记录实际看到的前后值。点表保持不变，不需要再次 apply。
- MQTT broker 测试凭证尚未提供，因此连接、掉线、重连与上报恢复验收未执行。
- 使用无凭证默认路径运行 `vgmqtt -h 192.0.2.1`，板端在网络连接前拒绝并报告 provision store 无 MQTT 凭证；该检查只验证缺凭证时的拒绝路径，不代表 MQTT 连接或恢复验收。原始记录见 [mqtt-missing-default.log](mqtt-missing-default.log)，SHA256 `e6feacf330dc8a15936dd3c935cd89632338a98847d6ea8458cd107f27e9de4e`。
- QA-04 串口失联原因仍未知；I8 丢帧滑窗复验通过不构成根因定位。完整 24 小时长测尚未开始。

原始构建、刷写、串口和存储日志保存在本机 `.debug/`，本记录列出各日志哈希；它们不会替代后续告警目视、MQTT 真机与完整长测证据。
