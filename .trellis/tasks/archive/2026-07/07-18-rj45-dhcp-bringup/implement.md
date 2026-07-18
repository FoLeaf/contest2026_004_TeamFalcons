# Implement: H750 RJ45 DHCP 联网最小切片

## Checklist

1. [x] 以 MB1381 H750XB-B01 原理图为准列出最终 `kconfig-tweak` 开关清单（MII、LAN8740A、PHY 地址 1、PHYSR、LPWORK、NETINIT_THREAD/DHCPC）。
2. [x] 核对 MII 与 QSPI-XIP 共脚；参赛仓 patch 跳过 PH2/CRS、PH3/COL 配置并补齐 `IFF_RUNNING` carrier 合同。
3. [x] 修改 `.debug/openvela_windows_build.sh`：在现有 `kconfig-tweak` 后追加网络开关；保证 `olddefconfig` 后无未解析依赖。
4. [x] 构建：优先 config 变更触发 clean；产出 `.debug/nuttx.bin` 等现有产物。
5. [x] 烧录板子；插网线：串口 `ifconfig` / 日志确认 IPv4；PC `ping`。
6. [x] 无网线冷启动：确认 UI + NSH 不长时间阻塞。
7. [x] 将实际生效的 Kconfig 摘要记入任务 notes 或简短 README 片段（可选，不强制新文档）。

## Validation commands

```bash
# 构建（WSL / 现有脚本）
bash .debug/openvela_windows_build.sh   # 或项目约定入口

# 板端串口 (115200)
ifconfig
ping <pc-ip>   # 若 SYSTEM_PING 已开

# PC
ping <board-ip>
```

## Risky files / rollback

| 路径 | 风险 |
|------|------|
| `.debug/openvela_windows_build.sh` / `scripts/windows_build_openvela.ps1` | 主变更；回滚删 NET tweak 块 |
| `scripts/openvela-eth-mii-stm32h750b-dk.patch` + apply 脚本 | MII/QSPI 共脚或 carrier 合同错误导致无链路；停用 apply |
| 应用代码 | **默认不改** |

## Review gate before start

- PRD 验收：串口 IP + ping；UI 不改；脚本落点；NETINIT_THREAD。
- design/implement 已覆盖 Kconfig 族与 board.h 风险。
- 用户批准后：`task.py start`，再实现。
