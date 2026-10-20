# RSP-03：推理参数与单点查数捷径

日期：2026-10-20。状态：第 1 项已测不采纳；第 2 项已实现但板端量化未完成（从站离线、模拟从站未连接）。

固件 SHA256 `95f8699db7bc7300a8d7fbc5929b2c0a12ffa6d2026d8e02ced785a12c4b0a7f`
（含点查捷径与 A/B 开关）。

## 一、推理参数：已测，不采纳

### 先确认接口实际支持什么

不猜参数，直接对端点探测：

```
GET /v1/models -> mimo-v2.5, mimo-v2.5-pro, mimo-v2.6-flash, mimo-v2.6-pro,
                  mimo-v2.5-asr, mimo-v2.5-tts, ...
```

逐个发送候选参数，看是否被接受、`reasoning_content` 是否真的消失：

| 参数 | 状态 | reasoning 字节 |
|---|---|---|
| 无 | 200 | 34 |
| `reasoning_effort: "none"` | 200 | **0** |
| `reasoning_effort: "low"` | 200 | 292 |
| `thinking: {"type":"disabled"}` | 200 | **0** |
| `enable_thinking: false` | 200 | 34（无效） |
| `temperature: 0.3` | 200 | 30（无效） |

只有 `reasoning_effort` 真正起作用，`enable_thinking` 与 `temperature`
对这个模型没有效果。

### 10 对交替 A/B

固定模型、提示词、工具 schema、输出预算；只改 `reasoning_effort`；
交替哪一臂先跑，避免时间漂移偏向一方。

中途发现一个测量缺陷：MiMo 把工具调用放在 `content` 里的 XML
（`<tool_call><function=...>`），`tool_calls` 字段为 `null`。第一轮
harness 没解析它，10 个用例全部误判为"未选工具"。加上 XML 解析后重测。

结果：

```
on    n=10 correct=10/10  total_ms median=2158.5 max=5441  tokens median=38.5
none  n=10 correct=10/10  total_ms median=1940.5 max=21491 tokens median=19.0
```

逐对差值：

```
pair  1 on= 2129 none= 21491 diff=+19362
pair  2 on= 1431 none=  2393 diff=  +962
pair  3 on= 1597 none=  1488 diff=  -109
pair  4 on= 5441 none=   939 diff= -4502
pair  5 on= 1611 none=  3582 diff= +1971
pair  6 on= 2224 none=  1165 diff= -1059
pair  7 on= 4003 none=  2970 diff= -1033
pair  8 on= 2188 none=  2802 diff=  +614
pair  9 on= 1726 none=  1346 diff=  -380
pair 10 on= 2818 none=   826 diff= -1992

median diff (none-on) = -244ms
pairs where none faster: 6/10
mean +1383ms  stdev 6562ms
```

**不采纳。** 中位数只快 244ms，胜率 6/10，标准差 6562ms 是效应量的
二十多倍。关掉推理确实把 completion tokens 从 38.5 降到 19，但整轮时间
由服务端 8 秒首字节与请求轮数主导（RSP-02 已证），省下的十来个 token
淹没在噪声里。正确性两边都是 10/10。

按"只有测得稳定收益才保留"的规则，不修改请求参数。

## 二、单点查数捷径：已实现，量化未完成

### 改动

一次"UPS 负载是多少"要两次模型往返：第一次选 `vg_point_read`，第二次
把工具已经算出的数字换个说法。第二次只做复述，却要再付一次服务端首字节
等待。

新增 `src/core/point_reply.c`：当本轮只有一个 `vg_point_read`、且工具
输出是一条成功的采样行时，直接按字段渲染回答，不再发起第二次请求。

工具输出形如：

```
vgquery: point id=ups_load name=UPS负载 value=42 unit=% ok=1 age_ms=900
```

渲染结果保留点名、数值、单位、采样时间；`stale=1` 时明确写出"采样已
过期"。数值原样回显，不四舍五入、不做单位换算、不重算——这条路径是为
了省一次往返，不能成为第二个数据源。

以下情况继续走模型：结果歧义、点表缺失、无采样、读取失败、缺少任一
字段。这些需要措辞而不是复述。

### A/B 开关

同一份固件带运行时开关，避免两次烧录引入差异：

```
vela> point_shortcut on|off
```

开关不写入持久配置，重启即恢复默认，测试设置不会留在生产路径上。

### 主机验证

`tests/test_point_reply.c` 覆盖 5 组：成功采样、过期采样、数值原样、
10 种必须 fall through 的形态、点名缺失时回退到 id。全部通过。

### 板端量化：未完成

冒烟测试暴露了阻塞：**从站离线**，工具输出是
`value=- reason=read_failed`，捷径按设计 fall through 到模型。冒烟记录：

```
point shortcut: on
http iter=0 ... tool=vg_point_read ...
http iter=1 ...              <- 第二次请求仍然发生
```

这是正确行为，但意味着当前条件下无法测出收益：只有成功采样才会命中
捷径，而从站离线时没有成功采样。

要完成量化需要板端 RS485 总线接一个在线从站。检查发现
`COM4`（模拟从站使用的串口）当前状态为 `Unknown`，USB-RS485 适配器
未连接，因此 `scripts/maintenance_rtu_slave.ps1` 无法启动。

**第 2 项的收益数字因此缺失，不宣称提速。** 已完成的部分只有实现与
主机回归。

## 三、当前结论

| 项 | 状态 |
|---|---|
| 推理参数对照 | 已测，10 对，不采纳（无稳定收益） |
| 单点查数捷径实现 | 完成，主机回归通过 |
| 单点查数捷径量化 | **未完成**，需要在线从站 |
| 正确性 | 推理 A/B 两边 10/10；捷径主机用例通过 |

固定快照与真实在线数据的区分：本轮推理 A/B 用的是固定提示词与工具
schema，不涉及实时点位；点查捷径的板端测量需要真实在线数据，当前没有。

连接超时与资源释放仍列账。未开展 HTTP/3、QA-04 或长测。
