# G 阶段最终镜像并发拒绝复验

记录时间：2026-10-19 10:49 至 10:54（Asia/Shanghai）

设备为 2026-10-18 23:48:40 构建的最终维护镜像，当前维护分支为 `1bb76af`。本次只改 COM7 模拟从站的 UPS 负载，不改点表，不执行 `vgpoint apply --confirm`，不复位设备。

## 基线与触发

只读基线确认：

- `uname -a` 为 `NuttX 0.0.0 2373ab4a45d Oct 18 2026 23:48:40`。
- Agent 为 `state=idle gen=11 owner=none`，最近回复仍是错误。
- 温度为 `24 C ok=1`，UPS 负载为 `40 % ok=1`。

10:51:16 向模拟从站下发 `value 5 0 701`，把 UPS 负载改为 70.1%；10:51:32 恢复 `normal`。板端事件为：

```text
10:51:20 id=ups_load action=raise kind=threshold value=70.1 thr=70
10:51:36 id=ups_load action=clear kind=threshold value=40 thr=0
```

温度全程保持 24°C。恢复后 `vgruntime dump` 显示 14 点在线，UPS 负载回到 40，事件状态为 `kind=none`；COM7 从站计数继续递增，`overrides=0 drops=0`。

## 并发拒绝

专用监视器轮询 `vgagent status`，观察到 advice 轮次运行后立即发送第二条手动请求：

```text
round: state=running gen=11 owner=advice
vgagent ask read ups_load current value
vgagent: queue failed: -16
round: state=running gen=11 owner=advice
```

这证明最终镜像在 advice 运行中拒绝并发手动轮次，原轮次所有权未被覆盖。该结论只覆盖轮次队列隔离；随后这条 advice 轮次本身在 90.080 秒后失败，`ret=0x52` 仍指向名称解析失败。因此本次不把 LLM 恢复或网络恢复记为通过。失败后串口继续输出心跳，事后 `vgagent status` 为 `state=idle gen=12 owner=none`，没有串口失联或重启。

## 原始记录

- 基线：`g-final-busy-baseline-20261019-104931.log`，SHA256 `b66dc4b1aa173ec1e3c9a34026d58b45bb0be69ff636b4d4d2c3a186012c770c`
- 触发和恢复时间：`g-final-busy-trigger-20261019-105116.txt`，SHA256 `ddb8cc80b25436db7ee62d69a6cb8e11354a45a98c8d0aba0ad948a502b61cf1`
- 监视器捕获：`g-final-busy-monitor-20261019-105116.log`，SHA256 `067ff85114da779f03db806edd271d93b3a96833fcaaca6d54f0ceae9a9878ed`
- 监视器完整包装输出：`g-final-busy-monitor-20261019-105116.log.monitor`，SHA256 `f21c4426af161894143777b5f9c2f1d1f51dc39abda3f4ba288561e6cdf7fe09`
- advice 结束监听：`g-final-busy-completion-20261019-105153.log`，SHA256 `703195b8f559cd32356240fb8801fdf88105b01140fede6357c6f47747da8267`
- 恢复后检查：`g-final-busy-postcheck-20261019-105344.log`，SHA256 `8f8d532a69b85489bf64b66b009a03d734191cb970821a01dd4ff98861e1b185`
- 监视器脚本：`g-final-busy-probe.ps1`，SHA256 `ea808463c0baa644b22da55820a72a047b2e1dc5354e681bab46252b325ff27a`

## 结论

最终镜像的并发拒绝复验通过。LLM 网络失败、DNS 差异原因、真实成功答复、MQTT 恢复、自动日报和 24 小时长测仍未完成。
