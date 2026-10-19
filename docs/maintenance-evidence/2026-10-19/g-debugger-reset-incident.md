# 调试器复位事件记录

记录时间：2026-10-19 11:15（Asia/Shanghai）

为只读读取 DNS 解析器表，尝试让 OpenOCD 不复位附着。虽然命令里重新执行了 `reset_config srst_only srst_nogate`，OpenOCD 输出仍显示 `connect_assert_srst` 保留；连接日志随后记录 `external reset detected` 和 halt 超时，命令未读到 DNS 表。

结果：板端在 11:15 左右重启。11:15:33 复核显示 `uptime` 为 0，固件仍是 23:48:40 镜像，Agent 回到 `gen=0 owner=none`，`vgcfg seq=6` 恢复，温度为 `24 C ok=1`。启动日志显示采集和 HMI 正常，但本次调试前的运行累计计数清零。

结论：这是调试器触发的外部复位，不属于 QA-04 自发失联，也不作为新固件验收证据。DNS 表读取失败，DNS 根因仍未定位。后续如需读取运行态数据，必须先修正 OpenOCD 无复位配置并单独评审暂停 CPU 的风险。

原始记录：

- OpenOCD 尝试：`g-final-dns-memory-20261019-111514.log`，SHA256 `aa9bcb271f9827d71243c619b7f4c26a396842220f0772c5a74ec4ac11e0a870`
- 复位后检查：`g-final-post-debug-reset-check-20261019-111533.log`，SHA256 `694a0df80bfba503ad28eb03edb0280a01d510f9533fbb154ee9b80bc3b8437d`
