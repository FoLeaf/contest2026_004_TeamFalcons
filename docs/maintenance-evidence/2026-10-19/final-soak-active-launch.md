# 24 小时长测启动记录

记录时间：2026-10-19 12:03（Asia/Shanghai）

在 11:57 启动失败后，改用 Windows `Start-Process` 独立启动正式长测。运行 ID 为 `final-soak-20261019T120155`，Windows PowerShell 进程 PID 14944 于 12:03:09 开始。脚本参数为 COM3、24 小时、900 秒周期、每轮 50 次存储写入、最少 14 点在线、单命令上限 240 秒。预计结束时间为 2026-10-20 12:03:09。

启动前未复位板端。脚本核对固件为 `NuttX 0.0.0 2373ab4a45d Oct 18 2026 23:48:40 arm stm32h750b-dk`，已确认点表 14 行，`temp` 显示名仍为 `温度验收`。COM7 从站由 Windows PID 4872 继续运行，12:03:41 从站状态为 70,695 请求、70,695 回复、`crc_scan=0`、无 drop、无覆盖。

12:05:14 完成第 1 轮：

```text
vgstoretest: OK rounds=50 bytes=25600 elapsed_ms=120630
CYCLE n=1 elapsed_s=122 uptime_s=2989 online=14/14
heap_used=305880 heap_maxused=408104
```

该轮点表基线一致，Agent idle 且 LLM 凭证 ready，14/14 点在线，温度 24°C，UPS 40% 且 `kind=none`，uptime 单调，堆记账一致。

本记录只宣布长测正在运行和第 1 轮通过。正式结论必须等待同一运行写出 `PASS` 和紧随的 `RUN_END status=PASS`；提前终止、控制台关闭或缺少 RUN_END 都只算不完整。脚本不检查告警、MQTT、LLM 调用、日报、语音和屏幕。

## 启动元数据

| 文件 | SHA256 |
|---|---|
| `final-soak-20261019T120155.start-metadata.txt` | `8e714a0732a6f34b93bf5f28cbc69c581d9d2e7dca90f068e1ecc6ea80260494` |
| `final-soak-20261019T120155.actual-end.utc` | `1cf37de5cbc12eff1c7de9c19cf084ed2d6d35adb388a896f839774ecf641eb0` |
