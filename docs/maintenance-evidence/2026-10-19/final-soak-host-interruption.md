# 长测宿主中断与板端复位记录

记录时间：2026-10-19 12:03 至 12:41（Asia/Shanghai）

`final-soak-20261019T120155` 完成 1 轮后在宿主侧终止。长测日志停在 12:05:13 第 1 轮 `CYCLE n=1`，Windows PID 14944 后续不存在，没有 `FAIL`、`PASS` 或 `RUN_END`。因此该运行不完整，不能计入连续 24 小时。COM7 从站 PID 4872 也被终止，`maintenance-slave-final2` 最后状态停在 12:14:31，累计 73,223 请求、73,223 回复、`crc_scan=0`。

12:40:58 板端复核显示 `uptime` 为 7 分 07 秒，推算复位时间约 12:33:46；复位原因没有现场证据，不归因于 QA-04。固件仍为 23:48:40 镜像。12:40:24 重启 COM7 从站后，12:41:11 复核显示 14 点全部在线，温度 24°C，UPS 40% 且 `kind=none`。总线启动累计 681 请求、373 成功、308 timeout、CRC 0；timeout 来自从站缺失窗口，不作固件失败证据。12:41 点表 `vgcfg seq=6 schema=1 committed=1 name=vgpoint`，温度显示名仍为 `温度验收`。

12:42 起 COM7 新从站恢复后继续 100% 应答。旧长测现场按不完整归档，板端复位后不续接旧运行；后续从当前稳定状态重新启动完整 24 小时长测。

## 原始记录哈希

| 文件 | SHA256 |
|---|---|
| `final-soak-interrupted-20261019T120155.log` | `e651c9999565d8ddc956ecd165f8032d84e0f3d1cc0e651b1454b89683c21ec3` |
| `final-soak-interrupted-20261019T120155.log.console` | `b3f9319d74fdd341a2fa0586b2af04379ebaf1696bd8b0dd5425afd45dc0d4b4` |
| `maintenance-slave-final2-interrupted-20261019.log` | `18d9869646be1fffb7c645414312c723a642f1c056941a816f973b1a02ae3a85` |
| `g-soak-interruption-board-check-20261019-124240.log` | `bc2014caa79cfa88b401a8506da3c6f674130a2d41e43527f75ba64da4bcfa5d` |
| `g-post-reset-config-check-20261019-124210.log` | `3a664169652f7729b3aaa146591ce56ca8abbe7af7775b1b629cef075a200070` |
