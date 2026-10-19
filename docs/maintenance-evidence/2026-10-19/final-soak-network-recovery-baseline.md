# 调试器复位后的网络恢复与长测启动前基线

记录时间：2026-10-19 11:19 至 11:30（Asia/Shanghai）

本记录只覆盖 11:15 调试器外部复位后的现场复核。DNS 和真实 LLM 请求在本次复核中恢复，但此前网络失败和 DNS 解析失败的根因没有定位，也没有执行物理断网和恢复实验。因此本记录不作为物理断网恢复验收，不关闭相关恢复项。

## 24 小时长测启动前基线

11:19:45 只读采样确认：

- 固件为 `NuttX 0.0.0 2373ab4a45d Oct 18 2026 23:48:40 arm stm32h750b-dk`。
- 板端 `uptime_s=284`，未再出现复位。
- 14 个点位全部在线，温度保持 `24`，Agent 为 `state=idle gen=0 owner=none`，LLM 凭证 ready。
- 总线启动累计为 `2364/2364` 成功；`crc`、`timeout`、`echo`、`other` 全为 0。
- 空闲 heap 约 6.7 MB。
- RJ45 地址为 `192.168.137.4`，网关 `192.168.137.1`，板端网关 ping 通过；MQTT 因测试凭证缺失而 down，属预期。

原始记录为 [final-soak-baseline-20261019-111945.log](final-soak-baseline-20261019-111945.log)。

## DNS 与真实 LLM 复核

11:21:19 板端 `nslookup token-plan-cn.xiaomimimo.com` 返回 `220.181.104.191`。这与此前板端 DNS 解析失败不同；重启前板端地址曾是 `192.168.137.48`，本次 DHCP 后变为 `192.168.137.4`。当前证据不足以判断恢复由地址变化、DHCP 租约、上游 DNS、网关状态还是调试器复位引起。

11:23:10 发起只读手动请求 `vgagent ask read ups_load current value`。Agent 调用一次只读工具，两次 TLS 握手记录均为 `handshake OK`，轮次最终为：

```text
END status=ok iters=2 tools=1 llm_ms=183020 elapsed=183s backend=0
reply: **UPS负载 (ups_load)**：当前 **40 %**
```

回复中的 `40%` 与当时模拟值一致，阈值说明也正确。轮次结束后 Agent 回到 idle。MQTT 仍因测试凭证缺失而 down。

原始记录为 [final-soak-dns-precheck-20261019-112119.log](final-soak-dns-precheck-20261019-112119.log) 和 [final-soak-llm-recovery-20261019-112310.log](final-soak-llm-recovery-20261019-112310.log)。

## 结论

本次复核证明调试器复位后的现场具备 DNS 解析和真实 LLM HTTPS 请求能力，可作为后续自动日报、告警解释和 24 小时长测的前置基线。它不解释此前 DNS 失败原因，不证明断网恢复能力，也不补齐 MQTT 凭证。COM7 模拟从站继续运行，点表保持已确认状态，温度保持 24°C。

## 原始记录哈希

| 文件 | SHA256 |
|---|---|
| `final-soak-baseline-20261019-111945.log` | `7e508ad5fd3fe286e6ae89ce18d5a857b3310e315174d9adf839389b9e7d82cb` |
| `final-soak-dns-precheck-20261019-112119.log` | `85c4d9e39b020baa3248da63922dcc1f3beeca1366f294da5bbd706d7b900741` |
| `final-soak-llm-recovery-20261019-112310.log` | `2affdac0dd9b8d95127c9753bb970d7e4dccd308752f8104e2f2193a7b6bb1df` |
