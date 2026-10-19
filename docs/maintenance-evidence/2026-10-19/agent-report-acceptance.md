# 板端运行报告与日报检查

记录时间：2026-10-19 09:24–11:03（Asia/Shanghai）

## 已有的当天日报文件

09:24 只读检查发现 `/data/velaguard/reports/daily-2026-10-19.md` 已存在，文件大小 546 B。文件头为 `AI-DAILY v1`、`date=2026-10-19`、`source=agent`，包含通信质量、点位在线和异常时间线三节，格式与日报 Skill 的要求相符。读取日志：[日报文件检查](daily-report-scheduled-check.log)，SHA256 `bbaf20c3b3509bc612ec2676892e07b0bcc9b56d0a285d685002eb8bfcde007c`。

该文件记录生成时运行 1 分 21 秒、总线 0/78 成功、14 点离线。稳定从站启动后，实时统计已经变化；09:35 板端快照为 14/14 在线、均为 `kind=none`。因此该文件只能证明当天 Agent 日报文件存在及格式正确，不能作为当前正常采集状态的日报验收。启动时的 `vghmi: daily report requested` 原始控制台行未捕获，自动触发也暂不按日志证据关闭。

## 10:58 自动触发复核

10:58:14 先把原 546 B 日报复制为 `daily-2026-10-19.baseline.md`，确认备份存在后删除当天原文件。板端随即输出：

```text
vghmi: daily report requested
[vgagent] round submitted: 124 bytes
[agent] Processing message from local_client:velaguard:13
```

这证明 HMI 调度器在缺少有效当天日报时自动发起了日报轮次。该轮在 90.090 秒后失败，`vela_tls` 记录 `ret=0x52`，trace 为 `END status=fail iters=1 tools=0 llm_ms=90090`；没有生成新的 `daily-2026-10-19.md`。11:02 从备份恢复原文件，确认大小和内容一致后删除备份。因此自动触发已证实，但正常采集状态下的 Agent 日报生成仍未通过，DNS 失败根因也仍未定位。

原始记录与脚本：

- 触发与失败监听：`g-final-daily-monitor-20261019-105814.log`，SHA256 `9f99abd55cae9a80c55fca977e8b75f4b2d6a7d2d590673192e74ffe041ce29d`
- 完整包装输出：`g-final-daily-monitor-20261019-105814.log.wrapper`，SHA256 `82d6d80f1606c0702de9a06527a0e8c55d8d24d087aacdd7c846964a815fdbd9`
- 备份、恢复和清理：`g-final-daily-restore-20261019-110249.log`，SHA256 `1075ed5375109b9c508c398cf19f16e3c1588891b4f983b8201998d583e61209`
- 监视器脚本：`g-final-daily-probe.ps1`，SHA256 `d9d82717668c848f14d1d26dd78834119bd169d030ea82d10de7557e6a96abb6`

## 实时运行报告查询

通过 `vgagent ask` 发起真实 LLM 轮次，请求只读调用 `vg_run_report` 且不写文件。trace 记录 `END status=ok iters=2 tools=1`，唯一工具为 `vg_run_report`，工具耗时 97.1 秒；完整串口记录见 [实时报告 ReAct 查询](live-run-report-query.log)，SHA256 `7e13180efbe70a027c1f4ce3916877eba4ba3989a1ff9cdb9b60eb15f809db00`。

NSH 状态探针只保存 `VG_AGENT_ROUND_REPLY_MAX=192` 字节，09:24 的长报告在 `最差从站` 后被截断，不能用该回显评估完整回答；限制定义见 `app/velaguard/vg_agent_round.h`。09:46 以短提示再次发起真实 LLM 轮次，trace 为 `END status=ok iters=2 tools=1`，唯一工具 `vg_run_report`；145 字节中文回答完整显示：`本次上电已运行约1小时2分钟，总线成功率100%（超时88次、CRC 0），各点位均在线，08:45 起多点位已恢复正常。` 原始记录见 [短答 ReAct 查询](live-run-report-concise-20261019.log)，SHA256 `8e27607e4c99915d0af951ff1edcbc8a576c3ec539ae46bc8c0a474687eed8f0`。短答的完整显示证明该次报告回答适配 192 字节回显上限，不代表长篇报告不会被截断。

09:47 直接调用只读诊断命令 `vgagent tool vg_run_report` 返回 `rc=0` 和完整报告：本次启动 1 小时 3 分、25,545/25,633 成功（显示 100%）、88 次超时、CRC 0、近期窗口全部正常，最差从站 1 为 3,642/3,662（99%）；异常时间线记录多个点位于 08:45 恢复在线。原始输出见 [最终实时报告工具结果](live-run-report-tool-final-20261019.log)，SHA256 `6d7cc56f64e60d2ee8b32045f92a0f1454ae45d1f3ac7a653edadab5859a4f78`。

09:35 复核显示 uptime 3044 秒、14 点全部在线且 `kind=none`、Agent idle、LLM 凭证 ready：[报告后板端状态](live-run-report-status.log)，SHA256 `262312aa1a28b8ae40c21e60ed4a9f71dde5499a7ef3ad3366e24059f0bfb4c1`。

09:47 状态快照显示 uptime 3803 秒、14/14 点在线且 `kind=none`、Agent `state=idle owner=none`、LLM 凭证 ready；保存的上一条回复与上述 145 字节短答一致。[最终报告后板端状态](live-run-report-status-final-20261019.log)，SHA256 `a5a08bd25a4592c3c3c794880df984bbf0b00f6bba67f3cdd02077433d41ed6a`。

## 验收边界

- 只读 `vg_run_report` 工具及真实 LLM 工具调用通过；09:46 的 145 字节中文短答完整捕获。09:24 长答仍受 192 字节上限截断，因此只确认简短回答完整，不确认长篇报告完整。
- 10:58 已证实缺少有效日报时 HMI 自动发起请求；但该轮因 `ret=0x52` 失败，原文件已从备份恢复。正常采集状态下的 Agent 日报生成和校验仍未通过。
- MQTT 测试凭证未提供；物理断开需要用户现场操作；温度 24→30→24 的屏幕刷新仍等待用户方便观察；E 阶段真实存储故障注入不在当前板上执行。
- QA-04 串口失联根因仍未知，完整连续 24 小时长测尚未开始。
