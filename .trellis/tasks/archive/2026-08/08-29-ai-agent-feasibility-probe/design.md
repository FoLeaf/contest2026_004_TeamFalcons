# 设计：ai_agent H750 探针

## 1. 配置策略

### 1.1 预设命名

建议新建 `stm32h750b-dk:velaguard-ai-probe`，**基于 `velaguard-net` 加法**（已有 RJ45/TCP/DHCP/DNS），而非从 lvgl 减法——探针需要 `net_test` 与 HTTPS。

备选：在 `velaguard-net` 上直接叠加 ai_agent Kconfig（更快，但污染 net 预设；探针期可接受，通过后再拆正式预设）。

### 1.2 Kconfig 最小集（来自推进方案 §4.4）

```text
CONFIG_EXAMPLES_AI_AGENT_VELA=y
# CONFIG_AI_AGENT_FEISHU is not set
# CONFIG_AI_AGENT_WEIXIN is not set
# CONFIG_AI_AGENT_MQTT is not set
# CONFIG_AI_AGENT_NODE is not set
# CONFIG_AI_AGENT_MCP is not set
# CONFIG_AI_AGENT_LVGL_UI is not set
CONFIG_AI_AGENT_TLS_CONN_POOL_SIZE=1
CONFIG_CRYPTO_MBEDTLS=y
CONFIG_NETUTILS_CJSON=y
CONFIG_NETUTILS_WEBCLIENT=y
CONFIG_RAMMTD=y
CONFIG_FS_LITTLEFS=y
```

### 1.3 裁剪参考

参照 `packages/ai_agent/defconfigs/esp32s3-eye/esp32s3-eye_defconfig` 与 `fix_esp32s3.sh`（若存在）—— mbedTLS 头文件冲突、密码套件、堆栈是已知雷区。

### 1.4 密钥

探针期 API key 编译期写入 `agent_secrets.h`（不提交仓库；本地 gitignore 或模板 + 文档说明）。

### 1.5 存储

- `/data/agent` 挂 RAMMTD + littlefs（`stm32_bringup.c` 已有逻辑，开 Kconfig 即可）
- 不要求 eMMC

## 2. 构建与交付形态

| 项 | 探针期做法 |
|---|---|
| defconfig | contest 仓 `scripts/` patch + apply，或 nuttx 公共仓分支（探针可本地 patch） |
| fix 脚本 | 若需 mbedTLS/voice 符号，contest 仓 `scripts/fix_stm32h750b_dk_ai_probe.sh` |
| build 入口 | `bash scripts/build.sh ai-probe`（新增 TARGET）或文档化手动 configure |
| 产物 | `.debug/nuttx.hex` + bootstub，板端 QSPI 烧录 |

遵守 `BOUNDARY.md` C2：公共仓改动最终走 PR；探针期可 contest 仓 scripted patch。

## 3. 验证顺序（严格按序，不要跳步）

```text
1. configure + make（AC1）
2. 烧录 + NSH 启动
3. ai_agent → vela>（AC2）
4. net_test（AC3）— 先确认 DHCP/网关，与 vgmqtt 环境一致
5. ask 你好（AC4）— 需有效 API key 与 outbound HTTPS
```

任一步失败：**停在该步**，记录日志/MAP/栈后再试下一步假设。

## 4. 风险与观测点（推进方案 §4.5）

| 风险 | 观测 / 缓解 |
|---|---|
| mbedTLS 首次启用 | 链接错误、握手栈溢出 → 参考 esp32s3 fix 脚本 |
| LLM resp_buf realloc 512KB | `mallinfo`/自定义日志确认分配在 SDRAM 而非 450KB AXI 堆 |
| voice/ 强制编译 | 链接 undefined → stubs 或 `-u` 弱符号补全 |
| pthread | 确认未设 `CONFIG_DISABLE_PTHREAD` |
| 任务栈 | ai_agent 主 32KB + outbound 16KB + 工具线程；watchdog 栈 fault 时加 `-g` 查 backtrace |
| DNS/HTTPS | net_test 通过后单独 curl/wget 或 agent 内置 net_test |

## 5. 探针报告模板

完成后写入 `research/probe-report.md`：

- 起始/结束时间与累计工时
- defconfig 差异摘要
- AC1–AC4 逐条 PASS/FAIL + 证据（日志片段、hex 大小）
- 失败时的根因分类（编译 / 栈 / 堆 / 网络 / TLS / LLM）
- 建议：继续正式集成 / 加大投入 / ai_chat / 赛道调整

## 6. 与 v3 产品路线图关系

```
探针(AC1-4) ──通过──► 阶段0地基(并行可部分提前) ──► 阶段1运营助手Agent
     │
     └──失败──► 20h停 + 决策点（推进方案 §4.6）
```

探针**不实现**告警解释、日报、Skill——仅证明 ai_agent 框架在 H750 上可运行。
