# F 阶段 I8 与 I9 板端验收记录

记录时间：2026-10-18 22:14 至 22:33（Asia/Shanghai）

对象为本轮候选固件
`NuttX 0.0.0 2373ab4a45d Oct 18 2026 21:50:34`，产品源码
`c20177f21e01bd70a4c96d12dc11095ebfcd08fd`。本页只记录 I8 和 I9；I8 未完成，
I9 通过。I10 在本文写入时尚未执行。该固件尚未包含集成分支后续提交，不能作为
最终冻结固件身份。

## I8：离线滑窗计数

前置从站日志为
[final-g-recovery-slave.log](final-g-recovery-slave.log)。从站从 22:05:17 起以
0 ms、`normal` 运行，22:14:32 写入一次 `drop 7 0 32`。控制前的 STATUS 累计请求
已经持续增长，满足至少 8 个正常采集采样的启动条件。

实际结果为：

- 从站仅记录到 `dropped=4`。
- 22:14:47 后请求和应答累计停在 `3758/3754`，之后不再增长。
- 板端同时失去 NSH 响应，无法取得第 5 次失败触发、16 个连续失败采样或恢复状态。
- 现场保存和恢复过程见 [G-I8 板端失稳记录](final-g-i8-board-failure.md)。

因此 I8 不能按清单判通过。没有板端事件证据证明第 5 个失败采样触发离线，也没有
drop 用尽后的成功恢复证据。该结果保留为未验证，不修改工况数据或点表来补写结果。

## I9：单次丢帧被重试吸收

22:30:57 写入并生效：

```text
CONTROL 2026-10-18T22:30:57.8515345+08:00
written=2026-10-18T22:30:57.7222765+08:00
line="drop 7 0 1"
effect=drop slave=7 reg=0 n=1
```

从站请求顺序：

```text
22:30:59.1260149 slave=7 fc=3 reg=0 qty=1 action=drop remaining=0
22:30:59.7680728 slave=7 fc=3 reg=0 qty=1 action=reply error=0 value=0
```

板端 22:31:19 的只读快照：

```text
vgpoint: VALUE id=door value=0 ok=1 unit=- age_ms=1280
vgruntime: point id=door slave=7 online=1 ... value=0 kind=none
```

`vgruntime` 事件缓冲区中没有新的 door raise 事件；最近的 door 事件仍是 22:27:40
的离线恢复 `clear`。从站累计最终保持 `crc_scan=0`，单次 drop 后只少一个应答，
下一次请求立即成功。

I9 结论：通过。它证明一次门磁应答丢失可被板端重试吸收，不能代替 I8 的连续失败
滑窗验收。

## 同一日志中的 Agent 轮次

[I9 原始日志](final-f-i9-drop-retry-20261018-2231.log) 还记录了同一固件上的一次
慢速 Agent 轮次：

```text
[trace:6ad5478b00000000] END status=ok iters=2 tools=2
llm_ms=116630 elapsed=123s backend=0
```

轮次写入 `/data/velaguard/reports/alarm_advice.txt` 1590 字节，并输出
`[vgagent] round finished: reply`。这是单次恢复观察，仍需后续工具失败回收、轮次
隔离和网络恢复验收，不能据此把 G 阶段全部判通过。

## 边界

- 原始板端日志：[final-f-i9-drop-retry-20261018-2231.log](final-f-i9-drop-retry-20261018-2231.log)
- 原始从站日志：[final-g-postreset-slave-20261018-2230.log](final-g-postreset-slave-20261018-2230.log)
- I8 未验证，I9 通过，I10 待执行。
- D-06 候选仍未 apply，真实用户确认闸门保持不变。
