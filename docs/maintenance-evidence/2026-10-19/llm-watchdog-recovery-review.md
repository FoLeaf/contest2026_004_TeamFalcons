# G 阶段 LLM watchdog 与连接恢复复核

记录时间：2026-10-19 09:56–10:13（Asia/Shanghai）

设备使用 2026-10-18 23:48:40 构建并于 10-19 烧录的最终维护镜像。复核期间没有改动点表、从站模拟值或固件，也没有复位设备。

## Watchdog 与轮次释放

尝试通过 `vgagent ask` 请求 Agent 用只读 `run_shell` 执行 `ps`。LLM 等待超过板端 120 秒预算；COM3 只读监听捕获：

```text
LLM watchdog: call took 122790 ms (limit 120s), treating as timeout
END status=timeout iters=1 tools=0 llm_ms=122790 elapsed=123s backend=0
round: state=done gen=7 owner=none
```

超时轮次没有派发 `run_shell`，因此这不是 `ps` 工具回收通过证据。随后缓冲的 `vgagent status` 返回 `state=idle gen=7 owner=none`，NSH 提示符恢复；本次不属于 QA-04 串口失联。

只读监听时间为 09:58:37–09:59:22，未发送任何命令，DTR/RTS 均为 false。日志中的中文错误回复被 Windows 输出编码替换字符，原文件保持不变；超时、trace 与状态字段为可读的 ASCII。完整记录见 [watchdog 完成监听](llm-watchdog-completion-observer-20261019.log)。

## 超时后的请求

超时后的一个只读温度点查询在实时命令输出中完成了 `vg_point_read`，耗时 88.27 秒；第二次 LLM 调用随后因 `ret=0x42` 失败，整轮 `END status=fail iters=2 tools=1`，没有生成最终答案。该次临时 PowerShell 监听器无法向 WSL UNC 路径写文件，因此没有 byte-exact 原始附件；这项只作为本会话的补充观察，不计作成功恢复证据。

之后两次无工具短请求分别在 09:59 后和等待路由退避后重试，仍以 `ret=0x42` 失败。再等待后，路由器日志显示自动恢复了 backend 0；下一次短请求约 90.08 秒后以 `ret=0x52` 失败。三次均返回 `state=done owner=none`，没有完整 LLM 答复。原始记录：

- [超时后的首个完整短请求](llm-followup-after-timeout-20261019.log)，SHA256 `49b48cbc1760c2b2b24993cc855a8122b65dc26d894b38475907e112581fd976`
- [退避后的重试](llm-followup-after-backoff-20261019.log)，SHA256 `ec620d1c162ebaa0c6d833d0d32ecb823077c79003f8784d776a3b859c5b100d`
- [路由自动恢复后的重试](llm-followup-after-idle-20261019.log)，SHA256 `88ec80bab680458e28d056f8681a01b0b121a413bcdefc6da6b43e4bf3006b83`

## 错误码与网络快照

`vela_tls.c` 记录的是 `-ret` 的十六进制值：[vela_tls.c](../../../../packages/ai_agent/src/infra/vela_tls.c#L283-L288)。本地 mbedTLS 定义 `-0x42` 为 `MBEDTLS_ERR_NET_SOCKET_FAILED`，`-0x52` 为 `MBEDTLS_ERR_NET_UNKNOWN_HOST`：[net_sockets.h](../../../../apps/crypto/mbedtls/mbedtls/include/mbedtls/net_sockets.h#L49-L66)。`mbedtls_net_connect()` 在 `socket()` 返回负值时返回前者，在 `getaddrinfo()` 失败时返回后者：[net_sockets.c](../../../../apps/crypto/mbedtls/mbedtls/library/net_sockets.c#L191-L205)。`0x44` 才表示 TCP `connect()` 失败。

代码没有把底层 `errno` 写进日志，所以无法从这些返回码确定 socket 创建或名称解析失败的更深层原因。检查到的连接池路径会释放陈旧连接并在失败时释放连接槽；本次代码阅读没有找到足以确认泄漏的证据。

10-19 网络只读快照显示 `eth0` 为 RUNNING，地址 `192.168.137.48/24`；到网关 `192.168.137.1` 的一次 ping 成功，域名 `token-plan-cn.xiaomimimo.com` 解析为 `220.181.104.192` 且一次 ping 成功。ICMP 成功不能证明 TCP/443 或 TLS 可用。`netstat -a` 在此镜像中不存在，板端没有通过该命令观察 socket 表的证据。

- [接口状态](llm-network-ifconfig-20261019.log)，SHA256 `84f804e3a0e524683c5e283258731a93114b59dc67f88b29827d86b2df52b86a`
- [网关与域名 ping](llm-network-ping-followup-20261019.log)，SHA256 `82b5161e831a396688010a9d4a371845e1bbf5801ceaa5a04bb96e56aff4d2f3`
- [netstat 不可用记录](llm-network-netstat-unavailable-20261019.log)，SHA256 `6f58edbdcb1aa81242181ce67f52f384b2486fee2efdf504b5fd0f094266796a`

## 结论

Agent 轮次在 watchdog 后释放，NSH 最终返回提示符；但超时后的 LLM 请求没有完成，后续观察到 socket 创建失败和名称解析失败。网络接口与 ICMP 在另一个时点可用，不能据此确认 TCP/TLS 已恢复，也不能确认 watchdog 本身造成后续网络错误。根因未定位，LLM 恢复验收未通过；当前保留未完成状态，不再在本轮重复发起请求。

关联监听与初始请求原始记录：

- [watchdog 请求起始](llm-watchdog-request-start-20261019.log)，SHA256 `2d2d47e51fa951e831d6b128223dd91c40e89957aa946df4cd3dddad542ddc69`
- [watchdog 完成监听](llm-watchdog-completion-observer-20261019.log)，SHA256 `6019a5b17da902203f68743e0a186651854f0b7fc53ce4260ddc1d89c248998c`
