# 长测启动进程中断记录

记录时间：2026-10-19 11:57（Asia/Shanghai）

`final-soak-20261019T115730` 由 WSL 后台方式启动，参数为 24 小时、900 秒周期、每轮 50 次存储写入、最少 14 点在线、命令上限 240 秒。脚本完成了固件身份核对和 14 行已确认点表基线；点表名为 `温度验收`，数量 14。

第一轮 `vgstoretest 50` 刚发出后，WSL 父进程退出导致 Windows PowerShell 子进程终止。日志停在第一条 `COMMAND ... vgstoretest 50`，没有 `CYCLE`、`PASS`、`FAIL` 或 `RUN_END`。因此该运行按脚本规则属于不完整，不能计入 24 小时长测，也不能继承任何后续 PASS。板端没有被复位，后续独立进程重新启动前需确认 COM3 已释放。

## 原始记录哈希

| 文件 | SHA256 |
|---|---|
| `final-soak-aborted-20261019T115730.log` | `33fe7a98a2ddfc7adb1969c7966762e663da1a7e1567d5be604f709c900f9cc4` |
| `final-soak-aborted-20261019T115730.log.console` | `90952781776d004b5cc9a554d3b6ecbde0398396a3a70dc0158cba001581972f` |
