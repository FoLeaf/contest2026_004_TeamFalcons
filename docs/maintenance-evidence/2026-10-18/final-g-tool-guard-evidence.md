# G 阶段工具边界补验

记录时间：2026-10-18 22:44 至 22:52（Asia/Shanghai）

本页整理 G 阶段的工具限流、禁止命令和忙轮次隔离证据。它只覆盖本页列出的板端
观察，不代表 G 阶段全部通过，也不替代真实告警解释、定时日报和 MQTT 恢复验收。

## 对象

- 固件：`NuttX 0.0.0 2373ab4a45d Oct 18 2026 21:50:34`
- 产品源码：`c20177f21e01bd70a4c96d12dc11095ebfcd08fd`
- 入口：COM3 NSH，只执行只读工具和状态查询
- 从站：COM7，9600 8N1，0 ms 回复
- 候选表仍未 apply，已确认表未改变

## 限流

通过 NSH 工具入口连续执行 22 次 `vgagent tool vg_point_read ups_load`：

- 第 1 至第 20 次返回 `vgquery: point ... ok=1`
- 第 21 和 22 次返回
  `vgquery: reason=rate_limited msg=调用过于频繁，请稍后再试`
- 22 次命令均获得 NSH 提示符，没有出现崩溃或串口失联

原始记录：

- [限流工况](final-g-rate-limit-20261018-2244.log)
- [限流后的工具审计](final-g-audit-after-rate-20261018-2245.log)

## 禁止命令

使用单引号保留 JSON 参数后执行：

```text
vgagent tool run_shell '{"command":"vgpoint"}'
```

结果为：

```text
[shell] Blocked dangerous command: vgpoint
tool run_shell rc=-1
{"error":"Command 'vgpoint' is blocked for safety"}
```

这说明命令闸门在工具实现对参数做解析后拒绝了 `vgpoint`，而不是依赖提示词
让模型自行避免。原始记录见
[禁止命令有效记录](final-g-forbidden-tool-singlequote-20261018-2247.log)。

## 忙轮次隔离

22:51:48 至 22:52:05 的忙轮次观察有效记录显示：

1. `vgagent status` 先显示 `state=idle gen=5 owner=none`。
2. `vgadvice` 提交告警解释轮次后，状态变为
   `state=running gen=5 owner=advice`。
3. 同一时刻执行 `vgagent ask read ups_load current value`，返回
   `vgagent: queue failed: -16`。
4. 再次执行 `vgagent status`，仍显示
   `state=running gen=5 owner=advice`。
5. 监视器记录 `BUSY-PROBE: matched running advice round`。

该窗口证明并发手动请求被拒绝，原轮次所有权仍为 `advice`。原始记录：

- [忙轮次监视器日志](final-g-busy-monitor-20261018-225148.log)
- [监视器触发和恢复时间](final-g-busy-monitor-trigger-20261018-225148.txt)

原始日志 SHA256：

- 监视器日志：`3ebde1d5232bb59bb5a21390e41f1bb3554b510b8a4612779518f0fd8a4f0ccb`
- 触发时间：`0252a9907adc44dcd6dc46c2fa4754bcc9c5edea160336b7932340172b2b02c1`

## 不可用尝试

以下文件保留用于追溯，但不能作为上述结论的通过证据：

| 文件 | 不可用原因 |
|---|---|
| `final-g-forbidden-tool-20261018-2246.log` | JSON 参数在传入 PowerShell 前被拆开，工具收到缺失 `command`，没有执行到命令闸门 |
| `final-g-busy-precheck-20261018-2248.log` | 只记录忙轮次前置状态和告警文件，没有并发请求结果 |
| `final-g-busy-isolation-20261018-2248.log` | `vgagent ask` 先被排队，随后 `vgagent status` 和 `vgruntime dump` 返回零长度，落入第三次 QA-04 失联窗口，不能证明并发请求被拒绝 |

`final-g-state-before-round-20261018-2242.log` 是阶段前的只读状态基线，可用于说明
Agent 和 HMI 当时已启动，不用于替代限流或忙隔离结论。

## 结论边界

- 已取得限流、禁止命令和一次忙轮次隔离的板端证据。
- 缺失 MQTT 凭证的稳态只说明当前配置边界，不能证明真实 MQTT 重连。
- 真实告警解释、定时日报、断网恢复、工具超时回收和 MQTT 恢复仍未完成。
- 本次没有执行 `vgpoint apply --confirm`。
