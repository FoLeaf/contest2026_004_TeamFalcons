# 探针工时日志

> ai_agent H750 探针 — 阶段 -1（推进方案 §4）

| 日期 | 阶段 | 耗时(h) | 备注 |
|------|------|--------:|------|
| 2026-08-29 | 0 准备 + 1 defconfig/build | ~1.5 | 新建 `velaguard-ai-probe` 预设；AC1 编译链接通过 |
| 2026-08-29 | 板端 AC2 + 网络 | ~0.5 | `vela>` 正常；eth0 DHCP OK；agent_loop 需先联网再启动 |
| 2026-08-29 | TLS 熵源修复 | ~0.5 | `net_test` 报 `No secure entropy`；defconfig 补 `CONFIG_DEV_URANDOM=y` |
| | | | **累计：~2.5 / 20h** |

## 环境快照

- openvela 分支 / manifest：本地 WSL，`nuttx` 在 `velaguard/eth-mii-stm32h750b-dk`
- 工具链：`prebuilts/gcc/linux-x86_64/arm-none-eabi`
- 网络环境（DHCP / 网关）：板端待测（与 vgmqtt 相同 RJ45 环境）
- Trellis 任务：`08-29-ai-agent-feasibility-probe`（in_progress）

## AC 进度

| AC | 状态 | 证据 |
|----|------|------|
| AC1 编译链接 | **PASS** | `bash scripts/build.sh ai-probe --clean` exit 0；`.debug/nuttx.hex` 已产出 |
| AC2 `ai_agent` → `vela>` | **PASS** | 板端进入 `vela>`；`Agent loop started` |
| AC3 `net_test` | **PASS** | `Handshake OK` + `SUCCESS! HTTP Status: 200`（www.baidu.com） |
| AC4 `ask 你好` | **PASS** | MiMo 回复：`你好！我是 AI Agent...`；latency≈3920ms |
| AC5 探针报告 | 待写 | 本文件 + `probe-report.md` |

## 构建产物（AC1）

```text
flash (QSPI XIP): ~544 KB（含 ai_agent + mbedTLS + velaguard-net 网络栈）
sram:             ~101 KB bss/data 区
```

## 首次链接修复

- 缺 `CONFIG_SYSTEM_SYSTEM=y` → `network_manager.c` 未定义 `system()`；已加入 defconfig。

## 下一步（严格按 implement.md 顺序）

1. 烧录 `.debug/nuttx.hex` + `qspi_bootstub.hex`
2. NSH：`ai_agent` → 确认 `vela>`（AC2）
3. `vela> net_test`（AC3）
4. 配置本地 `agent_secrets.h` 或 NSH `set_llm`，`vela> ask 你好`（AC4）
