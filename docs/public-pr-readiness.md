# 公共仓 PR 待发布清单

更新时间：2026-10-19 12:12（Asia/Shanghai）。本文只登记本地分支和证据，不表示已推送或已创建 PR。

## NuttX

- 本地分支：`velaguard/eth-mii-stm32h750b-dk`
- fork 分支：`FoLeaf/velaguard/eth-mii-stm32h750b-dk`
- 上游基线：`open-vela/nuttx` 的 `dev-ai-contest-2026`，当前基线 `dd92bcf4257`
- 待推送提交：4 个，HEAD `2373ab4a45d`
- 范围：STM32H750B-DK SDRAM 引脚保留、控制台线程栈配置、RS485 DIR 在 TX 完成中断释放、MQTT 凭证改用封存记录。
- 关键主机证据：`test_boot_stack` 和 `test_board_pins` 通过；完整参赛仓套件见 [2026-10-19 主机套件日志](maintenance-evidence/2026-10-19/host-tests-full-20261019-current.log)。
- 关键板端证据：RS485 0/5/20/150 ms 四档复验通过，D-02 覆盖改名复验通过；最终 23:48:40 镜像仍在长测中，PR 前需纳入长测最终结论。

## packages/ai_agent

- 本地分支：`velaguard/llm-tls-send-retry`
- fork 分支：`FoLeaf/velaguard/llm-tls-send-retry`
- 上游基线：`open-vela/packages_ai_agent` 的 `dev-ai-contest-2026`，当前基线 `31faed70f683`
- 待推送提交：6 个，HEAD `7dc0a88`
- 范围：shell 执行预算和输出治理、回复关联与迟到回调过滤、date 工具参数护栏和限流预留、文件写入末段软链接防护、VelaGuard 内建工具面收窄。
- 关键主机证据：`test_shell_runner`、`test_local_client`、`test_file_policy`、`test_agent_round`、`test_shell_output`、工具范围与限流专项均包含在 [2026-10-19 主机套件日志](maintenance-evidence/2026-10-19/host-tests-full-20261019-current.log)。
- 板端只读和禁用工具复核已归档；最终镜像的 LLM 成功轮次见 [告警解释成功答复复验](maintenance-evidence/2026-10-19/final-advice-success-acceptance.md)。

## 发布闸门

1. `final-soak-20261019T120155` 必须完成 24 小时，并留下同运行的 `PASS` 与 `RUN_END status=PASS`。
2. 长测结束后复核总线计数、从站 CRC、重启、掉点、串口失联、Agent 状态和资源趋势。
3. 推送和创建公共 PR 前需用户确认；当前 `gh pr list` 显示两个上游仓库均无对应 PR。
4. PR 创建后按 C2 交组委会 review；本文的本地证据不替代 CI、CLA 和上游 review。
