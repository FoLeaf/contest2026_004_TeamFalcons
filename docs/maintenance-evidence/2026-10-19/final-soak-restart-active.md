# 复位后重新启动 24 小时长测

记录时间：2026-10-19 12:43（Asia/Shanghai）

宿主中断和板端复位现场归档后，从复位后的稳定状态重新启动正式长测。运行 ID 为 `final-soak-20261019T124500`，Windows PID 3908 由 WMI `Win32_Process.Create` 创建，父进程不依赖当前 Codex 会话。COM7 从站为 Windows PID 21776，12:43:37 已累计 560 请求、560 回复，`crc_scan=0`。

长测参数保持 COM3、24 小时、900 秒周期、每轮 50 次存储写入、最少 14 点在线、命令上限 240 秒。基线固件为 23:48:40 镜像，已确认点表 14 行，温度显示名仍为 `温度验收`。

12:45:37 第 1 轮通过：

```text
CYCLE n=1 elapsed_s=124 uptime_s=700 online=14/14
heap_used=305504 heap_maxused=393448
```

12:45:37 从站累计 2101 请求、2101 回复，无 CRC、drop、抑制或覆盖。两个 Windows 进程在首轮后均存活。正式计时从约 12:43:22 开始，预计 2026-10-20 12:43:22 结束。只有同运行写出 `PASS` 和 `RUN_END status=PASS` 后才能判通过。

## 启动元数据哈希

| 文件 | SHA256 |
|---|---|
| `final-soak-20261019T124500.start-metadata.txt` | `f32a0beb79e5ab37ff76c581333480cacf6f24e208c6524d60dba00fd1950002` |
| `final-soak-20261019T124500.actual-end.utc` | `65fe7fe0ee29549fa8c778d0d8d8ba90b1f2b64612c2217f2b5f7cc3b612db82` |
| `final-soak-20261019T124500-start.json` | `4a1aef190e9ede1d236789836cd590500a53a01afc9149d875c9ba67dd4ab9cf` |
