# I8 连续失败滑窗复验

窗口：2026-10-18 23:41:49 至 23:44:03（Asia/Shanghai 主机时间）。

对象仍为 `NuttX 0.0.0 2373ab4a45d Oct 18 2026 21:50:34`，产品构建源码为
`c20177f`。本次已按用户确认把 `temp.name` 改为 `温度验收`，其余 14 点采集参数
与先前工况相同。COM7 为 9600 8N1、0 ms 从站；期间无其他 Modbus 主站，
无 OpenOCD/GDB 会话，无复位、重烧和点表修改。

## 观察方法

控制前保持 normal。从站逐请求日志在触发前包含 15 次连续门磁成功应答，超过滑窗
所需的 8 个正常采样。随后只写一次 `drop 7 0 32`，每 3 秒从 COM3 查询一次
`vgruntime dump`，共 40 次。观察器在未收到 NSH 提示符时会退出并保留板端现场，
退出时仅恢复从站 normal。

实际执行的 [PowerShell 观察器](qa04-i8-resumed.ps1) 已归档，
[板端原始输出](qa04-i8-resumed.log) 中含主机命令时间和控制时间。

## 结果

| 失败采样数 | 主机查询时间 | `boot_timeout` | 门磁状态 |
|---|---|---:|---|
| 0 | 23:41:56 | 0 | `online=1 kind=none` |
| 1 | 23:41:59 | 1 | `online=1 kind=none` |
| 2 | 23:42:02 | 2 | `online=1 kind=none` |
| 3 | 23:42:05 | 3 | `online=1 kind=none` |
| 4 | 23:42:08 | 4 | `online=1 kind=none` |
| 5 | 23:42:11 | 5 | `online=0 kind=offline` |

从站的第 9、10 次丢弃发生在 23:42:07.696 和 23:42:08.343，随后板端在
23:42:09 记录唯一一次 `door action=raise kind=offline`，对应第 5 个失败采样。
前 4 个失败采样均未进入离线。

32 次丢弃连续发生，中间没有门磁成功应答，形成完整的 16 个失败采样。第 32 次为
23:42:45.216、`remaining=0`；随后成功应答为 23:42:48.186，板端同秒记录
`action=clear ... reason=offline_recovered`。第 16 个失败采样仍为离线，首次成功
采样后恢复在线。39 份完整运行时快照中的其他 13 点均在线；`SAMPLE=5` 查询有 Agent 后台
输出插入，部分点行不完整，不能逐行判定该份查询的全部点位。最终 14 点均 `online=1 kind=none`，
`pending_alarm.txt` 已由板端删除。

40 次运行时查询及首尾查询均得到 NSH 提示符，内核 uptime 持续增长。本次未复现
QA-04，无故障文字和启动横幅，观察器退出码为 0。告警解释轮次在观察结束时仍为
`state=running owner=advice`，本页不把它计为解释生成通过。

## 从站证据边界

[从站逐请求摘录](qa04-i8-resumed-slave-excerpt.log) 按完整行原样截取自仍在追加的
`.debug/final-g-postreset-slave-20261018-2230.log`。摘录包含触发前 15 次门磁成功应答，
本次完整控制窗口，以及恢复 normal 的控制行。字节偏移、长度、截取时源文件大小和
哈希见 [摘录清单](qa04-i8-excerpt-manifest.json)，没有把摘录冒充整个源文件。
从站累计 `crc_scan=0`，本次 dropped 差值为 32；此前累计的 21 次丢弃不计入本工况。

## 结论

本次 I8 的滑窗计数和恢复行为通过。22:14 的失败尝试及其调试器现场仍保留于
[历史 I8 记录](final-f-i8-i9-board-acceptance.md) 和
[QA-04 失联记录](final-g-i8-board-failure.md)。一次正常复验不能关闭 QA-04，
也不代表最终补修固件已冻结或 24 小时长测已完成。
