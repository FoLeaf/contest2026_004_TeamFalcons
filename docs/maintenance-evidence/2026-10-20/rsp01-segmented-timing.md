# agent 响应链路分段计时（RSP-01）

日期：2026-10-20。状态：实现完成，主机回归通过，固件已编译；真机基线尚未采集。

本文件记录 RSP-01 的测量合同落地结果。执行顺序与验收目标见
[ai_agent 响应链路优化计划](ai-agent-response-optimization-plan.md)。

## 这轮改了什么

1. 新增 `packages/ai_agent/src/infra/agent_timing.{c,h}`。它只做两件事：
   定义一次尝试的分段结构，以及把结构格式化成一行日志。没有全局状态、
   没有线程局部变量，板端 `CONFIG_TLS_NELEM=0` 不影响它。
2. `vela_tls` 增加 `vela_https_request_timed()`，原 `vela_https_request()`
   直接转调它并传 `NULL` 计时。飞书、视觉、网页搜索等调用方行为不变。
3. `llm_chat_tools()` 增加可选出参，记录请求构造、每次尝试的传输分段
   和响应解析时间。429 重试只保留最后一次尝试的分段，另用
   `attempt` 和 `backoff` 说明付了几次。
4. 工具执行改为真正计时：单工具路径和并行工具路径都记录各自的起止。
   原 trace 行里 `tool=<名字> latency=<毫秒>` 的毫秒是 LLM 调用时间，
   工具在其之后才执行。现在分开输出：
   - 保留 `tool=` 与 `latency=` 字段名，值仍是 LLM 调用时间；
   - 新增 `tool_iter=... tool_ms=... out=... ok=...` 一行，值才是工具耗时。
5. 整轮 `elapsed` 从 epoch 秒差改成 `CLOCK_MONOTONIC` 毫秒差。

## 输出样例

一次成功请求会多出两行（字段顺序固定，`na` 表示该阶段未执行）：

```
[trace:3f21aa0c11d0b421] http iter=0 attempt=1 reused=1 proxy=0 dns_tcp=na(fused) setup=4 tls=na send=7 ttfb=9140 body=252 pool=0 build=61 parse=12 http=9464 backoff=0 total=9476 req=16384 rc=200 status=200 parse_ok=1
[trace:3f21aa0c11d0b421] tool_iter=0 tool=vg_run_report tool_ms=38 out=214 ok=1
```

字段含义：

| 字段 | 含义 |
|---|---|
| `reused` | 复用连接为 1；为 0 时 `dns_tcp`/`tls` 才有值 |
| `dns_tcp` | DNS 解析加 TCP 连接，**合并阶段**，后缀 `(fused)` |
| `setup` | TLS 初始化与随机数准备，握手之前 |
| `tls` | TLS 握手 |
| `send` | 请求头和请求体写入完成 |
| `ttfb` | 发送完成到第一个响应字节 |
| `body` | 第一个字节到响应体读完（含响应头） |
| `pool` | 连接池取槽与复用检查，含等锁 |
| `build` | 请求 JSON 构造与序列化 |
| `parse` | 响应 JSON 解析与字段提取 |
| `http` | 整个传输调用 |
| `total` | 整个 `llm_chat_tools` 调用 |
| `attempt`/`backoff` | 第几次尝试、本次尝试前退避了多久 |
| `req`/`rc`/`status` | 请求字节数、传输返回值、HTTP 状态码 |

## 刻意没做的事

- **没有拆分 DNS 与 TCP。** `mbedtls_net_connect()` 一次完成解析和连接。
  再解析一次会改变被测对象，得到的数也不是用户等待时间。合并阶段如实
  标注；确认它占主要时间后再单独评估拆分方式。
- **没有把 `ttfb` 说成模型首 token。** 当前链路非流式，首个字节是 HTTP
  状态行。
- **没有统计流式输出。** RSP-04 才有依据评估。
- **代理路径只报 `http` 总时长。** `proxy_conn_*` 把隧道、握手和读循环
  都封在内部，其余阶段标 `na`，不填推测值。
- **没有记录任何请求正文、响应正文、请求头或密钥。** 只有大小、状态码
  和耗时。

## 修正的历史误读

[运行报告验收](../maintenance-evidence/2026-10-19/agent-report-acceptance.md)
把 `tool=vg_run_report latency=97100ms` 写成工具耗时 97.1 秒。源码显示
该数取自 `llm_chat_tools()` 前后，工具在之后才执行，所以它是那一轮 LLM
调用耗时。历史日志原文不改写，误读以此处更正为准。

## 验证情况

- 主机回归 `packages/ai_agent/tests/test_agent_timing.c`：覆盖未测阶段报
  `na`、合并阶段标签、短缓冲拒绝、敏感字段不出现、32 位毫秒回绕。
- 竞赛仓主机套件 `make -C app/velaguard/host_tests test`：全部通过。
- `bash scripts/build.sh`：固件编译通过，烧录与真机采集未执行。

真机分段基线需要按计划 RSP-02 的方法采集，本轮不宣称已有性能数据。

## 构建环境备注

主仓 `nuttx/staging/libapps.a` 曾混入 65 个来自
`.vg-maintenance-closeout` 工作树的陈旧目标文件，导致链接时
`vg_agent_round.c` 重复定义。原因是用该工作树构建过固件、之后链接指回
主仓，旧成员没有被替换。清理方式：

```bash
cd ../nuttx/staging && ar d libapps.a $(ar t libapps.a | grep vg-maintenance-closeout)
```

这与本次计时改动无关，但会在同样的操作顺序下再次出现。
