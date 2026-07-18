# H750 RJ45 DHCP 联网最小切片

## Goal

在 MB1381 H750XB-B01 上启用板载 RJ45（LAN8740A / MII）+ DHCP，使板子接入局域网后能拿到 IP；开发者可在串口看到地址，并用 PC `ping` 验证连通。本切片不包含 ESP-01、MQTT、Modbus 与完整 network_manager。

## Background

- B01 原理图确认板载 Ethernet PHY 为 **LAN8740A（MII，地址 1）**；`stm32h750b-dk` README 的 LAN8742A/RMII 表格与实际硬件不符。
- `board.h` 已有部分 ETH/RMII GPIO 定义；当前产品 `.debug/nuttx.config` 中 **`CONFIG_NET` / `CONFIG_STM32H7_ETHMAC` 关闭**，`CONFIG_SCHED_LPWORK` 关闭。
- 产品构建入口：`configure.sh stm32h750b-dk:lvgl` + `.debug/openvela_windows_build.sh` 的 `kconfig-tweak`（QSPI boot、`velaguard_main`）。
- `velaguard_main` 调用 `nsh_initialize()` → `nsh_consolemain()`；`CONFIG_NSH_NETINIT` 开启时会 `netinit_bringup()`，**可不改应用代码即可自动 DHCP**。
- 同家族参考：`nucleo-h743zi/configs/netnsh`（LAN8742A）、`stm32h745i-disco/configs/netnsh`。
- 用户物理链路已就绪；首页「网络」仍为离线 stub。

## Decisions

| 项 | 决定 |
|----|------|
| UI「网络」 | **不改**；验收仅串口 + PC ping |
| 配置落点 | **仅参赛仓** `.debug/openvela_windows_build.sh` `kconfig-tweak` |
| 无网/DHCP 慢路径 | **`CONFIG_NETINIT_THREAD=y`**，后台拉网，不阻塞 UI/本地环 |

## Requirements

1. 固件启用板载以太网 MAC + PHY + IPv4 协议栈，启动后后台发起 DHCP。
2. 插上网线且链路 up 后，板子获得 IPv4 地址。
3. NSH/串口可查看网卡状态与 IP（`ifconfig` 等）。
4. 同网段 PC 可 `ping` 通板子 IP。
5. 无网线时系统仍能启动并显示现有中文首页；不崩溃。
6. 变更可复现：再次 full/incremental 构建仍带上网配置。

## Acceptance Criteria

- [x] 烧录后插网线，串口可见 `eth0`（或等价）获得非 `0.0.0.0` 的 IPv4。
- [x] PC 对板子 IP `ping` 成功（连续若干次应答）。
- [x] 无网线启动：UI/NSH 可用，不长时间卡死在网络初始化。
- [x] 现有中文首页/告警骨架仍能启动；「网络」可继续显示「离线」。
- [x] 关键 Kconfig 已写入 `.debug/openvela_windows_build.sh`，可重复构建。

## Out of Scope

- ESP-01、双模切换、完整 `network_manager`
- MQTT / AI Bridge 真连接
- UI 显示真实网络状态/IP
- DNS 业务使用（可不强制）
- Modbus / 本地安全环
- openvela 上游新增 `netnsh` defconfig

## Technical Notes (non-binding; see design.md)

- 最小开关族：`NET`、`STM32H7_ETHMAC`、`STM32H7_MII`、`ETH0_PHY_LAN8740A`、`NETINIT_DHCPC`、`NSH_NETINIT`、`NETINIT_THREAD`、`SYSTEM_PING`、`SCHED_LPWORK` 等。
- QSPI-XIP 与 MII 的 PH2/CRS、PH3/COL 共脚；全双工模式必须保留 PH2/PH3 的 QSPI 复用，并跳过未使用的 CRS/COL GPIO 配置。
