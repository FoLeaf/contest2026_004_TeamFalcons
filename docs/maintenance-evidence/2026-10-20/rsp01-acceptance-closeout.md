# RSP-01 验收收尾

日期：2026-10-20。状态：响应结束判定已修复并回归；失败路径已测到但未通过；恢复验证受网络环境影响未完成。

固件：`nuttx.hex` SHA256 `5ffcf87b1114a697d67ca62e10120ff857f6dd5171dc80aaf5a0ca469457db89`，
TARGET=`velaguard-lvgl`，烧录 13:21。

## 一、响应结束判定

### 发现的缺陷

`tls_read_response()` 与 `vela_http_post_json()` 的响应体循环只认
`Content-Length`。chunked 响应没有该头，于是在 keep-alive 连接上读完
终止块后继续读，直到 socket 超时（120 秒）才返回。

对照组实测：裸判定 `(cl >= 0 && pos >= cl)` 对一个已完整的 chunked 响应
返回不完整，代码会继续阻塞读。

### 修复

新增 `src/infra/http_body.h`，按 RFC 9112 6.3 的顺序判定结束条件：

| 分帧方式 | 结束条件 |
|---|---|
| `Transfer-Encoding: chunked` | 读到零长度块 |
| `Content-Length` | 读满声明字节数 |
| 都没有 | 对端关闭连接 |

两个读路径都改用该判定，并且每轮读取前先测一次完整性——初始片段可能
已经含有完整响应体，之后的读会等待不会到来的字节。没有长度信息的响应
额外打一条警告，避免把这种等待误读为生成慢。

### 主机实测

把未改动的 `vela_tls.c` 编到主机，链接 mbedtls 3.4.0，对本地可控 TLS
服务逐个用例跑（每个用例独立进程，避免连接池串扰）：

| 用例 | 耗时 | 结果 |
|---|---|---|
| Content-Length 完整 | 0.094s | 立即返回 |
| chunked 完整 | 0.094s | 立即返回，解码后 20 字节正确 |
| chunked 分两次到达 | 0.895s | 等于服务端发送间隔，无额外等待 |
| 连接关闭 | 0.094s | 立即返回 |
| Content-Length 截断 | 5.1s | 等满超时，符合预期 |
| chunked 截断 | 5.1s | 等满超时，符合预期 |
| 无分帧 keep-alive | 5.1s | 只能等超时，符合预期 |

主机测试的 socket 超时设为 5 秒，便于区分"立即返回"和"等满超时"。

`tests/test_http_body.c` 覆盖 10 组判定用例，包括块数据里的 `0` 字节
不被误判为终止块、长度行被读断时不算完整。

### 对板上 84 秒等待的影响：没有

板上重新采集后 `body=85470` 仍然出现，且没有触发新增的"无长度信息"
警告。这说明真实服务确实发送了 `Content-Length`，那 84 秒是在等服务端
把整篇告警建议生成完，不是分帧缺陷。

**上一轮把 84 秒归因于分帧是错的，这里更正。** 该缺陷真实存在且已修复，
但它不是板上长等待的原因。板上长等待的性质留待 RSP-02 用服务端或主机
对照确认。

## 二、失败路径

### 采用的方式

把 LLM 端点临时指向 `https://192.0.2.1/v1`（TEST-NET-1，RFC 5737 保留
地址，不会路由）。这比 `vgnet inject` 可靠：后者只改策略健康位，实测
不断开真实连接，此前那轮测试因此没有触发失败路径。

配置写在 eMMC 加密存储里，测试后用 `provision-llm-from-secrets.ps1`
加复位恢复，不经过日志或屏幕。

### 实测结果

两次失败请求都产出了完整计时行：

```
http iter=0 attempt=1 reused=0 proxy=0 dns_tcp=20(fused) setup=10 tls=na \
     send=na ttfb=na body=na pool=60 build=0 parse=na http=81580 \
     backoff=0 total=81590 req=9120 rc=-1 status=0 parse_ok=0
END status=fail iters=1 tools=0 llm_ms=81590 tool_ms=0 elapsed_ms=81640
```

| 验收项 | 结果 |
|---|---|
| 计时行完整 | 通过：`rc=-1`、`status=0`，未执行的阶段标 `na` |
| 请求有界结束 | **有条件通过**：81 秒后返回，没有挂死 |
| Agent 回到可接受状态 | 通过：失败后第二次 `ask` 产生了独立 trace 与回答 |
| 资源释放 | 未直接观测；第二次请求能成功发起间接支持 |
| 恢复后请求成功 | **未完成**：见下 |

### 未通过项：连接失败耗时 81 秒

`dns_tcp` 只有 20ms，说明解析和发起连接很快。真正的 81 秒在
`mbedtls_net_connect()` 内部：它用阻塞 connect，没有超时，
SYN 重传耗尽才返回。板上 `CONFIG_NET_TCP_MAXRTX=8`，重传退避累加
到约 81 秒。

这项**没有修复**。可选方向有两个，都超出本轮"验收收尾"的范围：

- 调整内核 `CONFIG_NET_TCP_SYNMAXRTX`：影响所有 TCP 连接，不只是 LLM。
- 在 `tls_ctx_connect()` 里改为非阻塞 connect 加超时：属于 RSP-03
  的连接层改动，需要单独评估与回归。

本轮只记录现象与成因，不擅自改动。

### 429 重试：未验证

本轮没有可控的 429 服务端。计划要求"429 用可控测试服务验证"，本轮
未完成。已确认代码路径为 `llm_chat_tools()` 的
`AGENT_LLM_MAX_RETRIES=3`，退避 `AGENT_LLM_RETRY_BASE_SEC << (retry-1)`，
计时行会记录 `attempt` 与 `backoff`，但没有实际触发。

## 三、恢复验证未完成

失败路径测试后，`provision-llm-from-secrets.ps1` 已成功恢复配置
（`vgagent status` 显示 `credentials=ready`，`round: state=idle`）。
但随后板端网络出现中断：

- 板端 IP 从 `192.168.137.205` 变为 `192.168.137.64`
- `ping 192.168.137.1` 无响应，并持续出现
  `WARNING: Ignoring ICMP reply with ID N. Expected 26`
- TLS 握手报 `NET - Sending information through the socket failed`
- 之后多次重试都拿不到完整回复

Windows 侧 ICS 地址仍在（`192.168.137.1/24`），判断是板端与主机共享
网卡之间的链路问题，与本次固件改动无关。**恢复后请求成功这一项因此
没有取到证据，不标记通过。**

## 四、计时分辨率

按要求保留系统 tick 配置不动：`CONFIG_USEC_PER_TICK=10000`，
`CONFIG_SCHED_TICKLESS` 未开。计时分辨率 10ms。

本轮实测再次确认：所有时长字段都是 10 的整数倍，没有一个例外。因此：

- 零值表示"低于分辨率"，不表示零耗时。
- 不做毫秒级性能比较。
- 秒级瓶颈的判断不受影响。

## 五、构建

构建前确认 `apps/libapps.a` 与 `nuttx/staging/libapps.a` 两处归档都没有
`vg-maintenance-closeout` 成员（均为 0），构建后再查仍为 0。上一轮
记录的两处陈旧归档已清除。

构建来源：`packages/ai_agent` 分支 `velaguard/rsp01-response-timing`，
竞赛仓分支 `codex/ai-agent-response-timing`。

## 结论

| RSP-01 完成条件 | 状态 |
|---|---|
| 正常路径完整时序 | 通过 |
| 失败路径完整时序 | 通过 |
| 重试路径完整时序 | **未通过**（429 未触发） |
| 不记录密钥或完整请求正文 | 通过（已扫描） |
| 请求有界结束 | **未通过**（81 秒，B 项已记录成因） |
| 恢复后请求成功 | **未完成**（板端网络中断） |
| 响应结束判定正确 | 通过（缺陷已修，主机与板端均验证） |

RSP-01 仍不标记通过验收。未开始 RSP-02、QA-04 排障或长测。
