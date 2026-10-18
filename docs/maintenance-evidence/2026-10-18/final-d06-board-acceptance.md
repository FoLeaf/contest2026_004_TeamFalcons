# D-06 最终固件板端试读记录

记录时间：2026-10-18 22:10（Asia/Shanghai）

## 对象

- 参赛仓分支：`codex/maintenance-closeout`
- 产品源码提交：`c20177f21e01bd70a4c96d12dc11095ebfcd08fd`
- 固件内嵌构建时间：`Oct 18 2026 21:50:34`
- 板端 `uname -a`：
  `NuttX 0.0.0 2373ab4a45d Oct 18 2026 21:50:34 arm stm32h750b-dk`
- `nuttx.bin` SHA256：
  `02e745bdd268bb6695b23e148045db3fcd237742752919d6cf7b06d76c748db4`

完整多仓来源、配置哈希、产物大小和烧录记录见
[D-06 构建来源](final-build-provenance-after-d06.md)。

## 修复内容

`vgpoint test` 原先只使用非阻塞总线获取。HMI 每 200 ms 采集一次，14 点从站
以 0 ms 回复时，一次采集轮次可能超过旧的 1000 ms 等待上界，命令仍会返回
`bus_busy`。

本轮把等待上界改为 3000 ms。命令继续使用公共总线互斥区，没有绕过锁，也没有
增加第二个 Modbus 主站。主机回归先持锁 1200 ms，再调用真实 `cmd_test`，
因此旧实现会失败，新实现会等锁释放后完成试读。

## 修复前板端现象

未包含该补修的固件在周期采集中反复执行试读，只记录到
`vgpoint: ERR cmd=test code=bus_busy msg=rs485_busy`。这些失败保留在
[final-d06-candidate-tests.log](final-d06-candidate-tests.log)，不能用后续
成功覆盖。

## 修复后试读

烧录补修固件后，在 COM7 从站以 0 ms 回复、周期采集持续运行的状态下连续执行
8 次 `vgpoint test`：

- 8 次均返回 `vgpoint: OK cmd=test table=candidate n=14`
- 共 112 条 `vgpoint: READ`，全部 `ok=1`
- 未出现 `bus_busy`
- 同一窗口内从站最终记录 2264 个请求和 2264 个应答
- 从站 `crc_scan=0`

原始记录：

- [8 次试读与运行时统计](final-d06-candidate-tests-after-fix.log)
- [COM7 从站请求和应答](final-d06-slave-after-fix.log)
- [最终固件 D-02 隔离改名复验](final-d02-recheck-after-d06.log)

## 候选差异与人工确认边界

候选表与已确认表均为 14 点。唯一差异是：

| 表 | `temp.name` |
|---|---|
| 候选表 | `温度验收` |
| 已确认表 | `温度` |

试读后的只读复查确认：

- `vgpoint list -c` 仍返回 `table=candidate n=14`
- `vgpoint list` 仍返回 `table=committed n=14`
- 已确认表首点名称仍为 `温度`
- `vgcfg dump` 仍为 `seq=6 ... committed=1`

截至本记录写入时，真实用户尚未确认 `apply`。因此本页只证明候选试读成功，
不证明候选已经生效。确认后的检查顺序必须是：

1. 真实用户明确确认本次候选和试读结果。
2. 单独发送 `vgpoint apply --confirm`。
3. 核对落盘候选或已确认表、运行时点表、首页模型和正常采集。
4. 核对 MQTT 点表与已确认表一致。
5. 复位并核对重启持久化。

恢复原显示名也必须重新生成候选、试读、展示并等待新的真实用户确认，不能复用
本次确认。
