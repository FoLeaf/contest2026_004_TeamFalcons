# 板端运行报告与日报检查

记录时间：2026-10-19 09:24–09:35（Asia/Shanghai）

## 已有的当天日报文件

09:24 只读检查发现 `/data/velaguard/reports/daily-2026-10-19.md` 已存在，文件大小 546 B。文件头为 `AI-DAILY v1`、`date=2026-10-19`、`source=agent`，包含通信质量、点位在线和异常时间线三节，格式与日报 Skill 的要求相符。读取日志：[日报文件检查](daily-report-scheduled-check.log)，SHA256 `bbaf20c3b3509bc612ec2676892e07b0bcc9b56d0a285d685002eb8bfcde007c`。

该文件记录生成时运行 1 分 21 秒、总线 0/78 成功、14 点离线。稳定从站启动后，实时统计已经变化；09:35 板端快照为 14/14 在线、均为 `kind=none`。因此该文件只能证明当天 Agent 日报文件存在及格式正确，不能作为当前正常采集状态的日报验收。启动时的 `vghmi: daily report requested` 原始控制台行未捕获，自动触发也暂不按日志证据关闭。

## 实时运行报告查询

通过 `vgagent ask` 发起真实 LLM 轮次，请求只读调用 `vg_run_report` 且不写文件。trace 记录 `END status=ok iters=2 tools=1`，唯一工具为 `vg_run_report`，工具耗时 97.1 秒；完整串口记录见 [实时报告 ReAct 查询](live-run-report-query.log)，SHA256 `7e13180efbe70a027c1f4ce3916877eba4ba3989a1ff9cdb9b60eb15f809db00`。

NSH 状态探针只保存 `VG_AGENT_ROUND_REPLY_MAX=192` 字节，报告答案在 `最差从站` 后被截断，不能用这段回显评估完整回答；限制定义见 `app/velaguard/vg_agent_round.h`。随后直接调用只读诊断命令 `vgagent tool vg_run_report` 返回 `rc=0` 和完整报告内容，证明板端统计工具可读取。09:33 报告为本次启动 49 分钟、总线 19599/19687 成功、88 次超时、CRC 0、近期窗口全部正常；原始输出见 [实时报告工具结果](live-run-report-tool.log)，SHA256 `371382100a4993582e2b237486675dca64464161bfbc9d8ed2b6a2b6e13d524f`。

09:35 复核显示 uptime 3044 秒、14 点全部在线且 `kind=none`、Agent idle、LLM 凭证 ready：[报告后板端状态](live-run-report-status.log)，SHA256 `262312aa1a28b8ae40c21e60ed4a9f71dde5499a7ef3ad3366e24059f0bfb4c1`。

## 验收边界

- 只读 `vg_run_report` 工具及真实 LLM 工具调用本次通过；NSH 探针的短回显不构成完整自然语言回答验收。
- 当天 Agent 日报文件存在且格式正确，但快照处于开机早期离线阶段；需在正常采集状态下获得并校验一份日报，再关闭日报验收。
- MQTT 测试凭证未提供；物理断开需要用户现场操作；温度 24→30→24 的屏幕刷新仍等待用户方便观察；E 阶段真实存储故障注入不在当前板上执行。
- QA-04 串口失联根因仍未知，完整连续 24 小时长测尚未开始。
