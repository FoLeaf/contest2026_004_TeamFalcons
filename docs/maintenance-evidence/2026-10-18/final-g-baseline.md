# G 阶段板端只读基线

采集时间：2026-10-18 22:02 至 22:06（Asia/Shanghai）

## 固件与现场

- 固件：`NuttX 0.0.0 2373ab4a45d Oct 18 2026 21:50:34`
- 产品源码提交：`c20177f21e01bd70a4c96d12dc11095ebfcd08fd`
- 采集入口：COM3 NSH，只执行查询命令
- COM7 从站在采集开始时已停止，因此本次同时记录了全静默现场

## 只读结果

- `vgagent status`：`state=running gen=0 owner=advice`，LLM 凭证
  `credentials=ready provision_file=present`
- `vgprovision mqtt status`：`MISSING (err -2)`
- `vgnet status`：RJ45 链路上、Ping 正常，`mqtt=down`；WiFi `assoc=0`
- `vgagent tools`：可见 `vg_point_read` 和 `vg_run_report` 两个只读工具
- `vgpoint list`：已确认表 14 点，`temp.name=温度`
- `vgpoint list -c`：候选表 14 点，唯一差异仍为 `temp.name=温度验收`
- `ps`：HMI、Agent、Agent 工作线程和 ESP8266/netinit 线程均存在
- `/data/velaguard/reports`：存在 `daily-2026-10-18.md`、
  `alarm_advice.txt` 等报告文件
- 全部从站静默后 14 点均在滑窗阈值后进入 `kind=offline`，
  `/data/velaguard/pending_alarm.txt` 存在

## 恢复复验

22:05 重新启动 COM7 从站，使用 0 ms 回复。随后只读复查显示：

- `vgpoint get` 14 点全部 `ok=1`
- `vgruntime dump` 14 点全部 `online=1 kind=none`
- `pending_alarm.txt` 已由 HMI 删除，`ls` 和 `cat` 均返回 `stat failed: 2`
- 全静默分组分别在 `t_boot_s=500` 和 `t_boot_s=519` 触发；22:05 恢复时
  两组均产生 `clear kind=offline reason=offline_recovered`

全静默触发距离从站停止约 77 至 96 秒，与代码推导的 5 个失败采样、
全静默单轮约 16 秒相符。该结果是本次现场观察，不替代后续按工况编号保存的
F 阶段阈值验收。

## G 阶段判断

可直接继续的只读项：

- 缺失网络、缺失 MQTT 凭证时的稳态和恢复行为
- 只读工具失败后的回收、轮次隔离、真实告警解释和日报文件检查

仍缺少的前置条件：

- MQTT 凭证当前缺失，真实 MQTT 重连和 MQTT 候选配置通道无法验证
- WiFi `ssid=19y assoc=0`，静态 WiFi 口令问题仍未解决
- 真实告警解释轮次在本基线采集时仍在运行，必须等轮次结束后检查文件、
  审计日志和下一次轮次是否可正常提交
- 物理拔线、屏幕目视仍需要真实人工操作，不能用从站静默和模型状态代替

原始记录：

- [只读基线](final-g-baseline-20261018-2202.log)
- 从站恢复日志在恢复与后续 F 阶段工况完成后单独冻结

## 当前人工节点

D-06 候选仍未 apply。真实用户确认前，不得单独发送
`vgpoint apply --confirm`。本次基线不是对候选的确认。
