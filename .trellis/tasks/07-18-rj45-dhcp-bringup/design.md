# Design: H750 RJ45 DHCP 联网最小切片

## Boundaries

| In | Out |
|----|-----|
| 参赛仓构建脚本启用 ETH + DHCP | ESP-01 / network_manager |
| 必要时板级 ETH 引脚/bring-up 补丁（参赛仓 scripts，对齐 QSPI 补丁模式） | MQTT / UI 网络状态 |
| 串口 `ifconfig` + PC `ping` 验收 | 上游 `stm32h750b-dk:netnsh` defconfig |

## Architecture

```
boot → board_late_initialize / ETH driver register (arm_netinitialize)
     → velaguard_main
         → nsh_initialize()  [CONFIG_NSH_NETINIT]
              → netinit_bringup()  [CONFIG_NETINIT_THREAD: 后台线程]
                   → ifup eth0 + DHCPC
         → UI task (不变)
         → nsh_consolemain()  [串口可 ifconfig / ping]
```

不改 `velaguard_app` 业务逻辑；依赖已有 `nsh_initialize()` 挂钩。

## Config contract (kconfig-tweak)

在 `.debug/openvela_windows_build.sh` 现有 tweak 块中追加最小集合（名称以实现时 `olddefconfig` 解析为准）：

**驱动 / 接口（MB1381 H750XB-B01）**
- `CONFIG_STM32H7_ETHMAC=y`
- `CONFIG_STM32H7_MII=y`、`CONFIG_STM32H7_MII_EXTCLK=y`
- `CONFIG_ETH0_PHY_LAN8740A=y`、`CONFIG_STM32H7_PHYADDR=1`
- PHY 状态寄存器族：`STM32H7_PHYSR*` / `ALTCONFIG` 对齐 LAN8740A
- `netdev_carrier_on()`：成功 ifup 后设置 `IFF_RUNNING`，否则 UDP/DHCP 在发包前返回 `EHOSTUNREACH`

**协议栈**
- `CONFIG_NET=y`、IPv4、ARP、ICMP、TCP、UDP（ping 需要 ICMP）
- `CONFIG_NET_ETH_PKTSIZE=1500`
- `CONFIG_SCHED_LPWORK=y`（以太网驱动/工作队列常见依赖）

**启动与 DHCP**
- `CONFIG_NETUTILS_NETINIT` / `CONFIG_NSH_NETINIT=y`（NSH 默认随 NET 开启）
- `CONFIG_NETINIT_DHCPC=y`
- `CONFIG_NETINIT_THREAD=y`（无网不阻塞）
- `CONFIG_NETINIT_NOMAC=y` + 软件/固定 MAC 或 UID 策略（板通常无烧录 MAC；实现时优先可复现的固定测试 MAC 或 UNIQUEID，若 UNIQUEID 代价大则 SWMAC）
- `CONFIG_SYSTEM_PING=y`、`CONFIG_SYSTEM_DHCPC_RENEW=y`（串口排障）

**刻意不加（本切片）**
- Telnetd、webclient、discover、完整 DNS 业务（`DNSCLIENT` 可选，不验收）

## Board-level risk

H750 `board.h` 的原始 MII 引脚与 B01 原理图一致，但 PH2/MII_CRS、PH3/MII_COL 与双 QSPI bank 2 IO0/IO1 共脚。QSPI-XIP 运行时不得把它们切换为 Ethernet AF；协商为全双工后 CRS/COL 不参与数据传输。

实现策略：

1. 参赛仓 patch 在 QSPI boot 模式定义 `BOARD_ETH_MII_NO_CRS_COL`，驱动跳过这两个 GPIO；
2. 同一 patch 在成功 `stm32_ifup()` 后调用 `netdev_carrier_on()`；
3. **不**在本任务向 openvela 上游提 netnsh defconfig。

## Compatibility

- 保持 `CONFIG_STM32H750B_DK_QSPI_BOOT`、`velaguard_main`、LVGL 首页路径不变。
- 启用 NET 后镜像变大：仍走 QSPI XIP，需关注链接/运行是否 OOM；若失败优先减 descriptor/关闭 debug net。

## Validation shape

1. full 或 config-change 触发 clean 构建 → 烧录
2. 插网线：串口 `ifconfig` 见 IPv4
3. PC `ping <ip>`
4. 拔网线冷启动：UI 仍起、NSH 可用

## Rollback

- 从构建脚本移除 ETH/NET tweak 块并 full rebuild 即回到离线基线。
- 若有 ETH MII/QSPI patch，停止 apply 脚本调用即可。
