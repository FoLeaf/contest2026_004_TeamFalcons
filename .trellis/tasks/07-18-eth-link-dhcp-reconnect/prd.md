# 修正 B01 以太网链路状态与 DHCP 重连

## Goal

让 MB1381 H750XB-B01 的 `eth0` 状态真实反映 LAN8740A 物理链路，并在
RJ45 拔出/插回后自动完成离线与 DHCP 恢复，避免 `RUNNING` 和旧 IPv4
地址长期伪装为在线。

## Background

- 前一任务已验证 B01 使用 LAN8740A、完整 MII、PHY 地址 1，并能通过
  DHCP 获取地址及双向 ping。
- 当前 STM32H7 补丁在 `ifup()` 成功后无条件调用
  `netdev_carrier_on()`；拔线路径没有 `netdev_carrier_off()`，因此
  `ifconfig` 仍显示 `RUNNING`。
- NuttX STM32H7 驱动虽然暴露 `SIOCMIINOTIFY`，但
  `stm32_phyintenable()` 仍是 `#warning Missing logic` / `-ENOSYS`；现成
  `CONFIG_NETINIT_MONITOR` 依赖 PHY interrupt，不能直接启用。
- B01 原理图 PDF 暴露的网络名为 `MII_TX_ER_nINT`，该脚具有复用语义；
  当前板级代码没有可验证的 PHY interrupt GPIO 定义。规划基线因此是
  低频 MDIO 读取 `MII_MSR_LINKSTATUS`，除非后续证据证明中断路径可用。
- `netdev_carrier_off()` 会清除 `IFF_RUNNING`、通知 netlink/clients、清理
  ARP；它不会自动删除 DHCP 写入的 IPv4、网关和掩码。

## Requirements

1. 拔出 RJ45 后，在有限时间内清除 `IFF_RUNNING`，`ifconfig` 不再报告
   物理链路在线。
2. 插回 RJ45 后，在有限时间内重新协商链路并自动恢复 DHCP，无需重启
   或手工执行 `renew`。
3. 链路监控必须后台运行，不阻塞 VelaGuard UI、触摸、采集循环或 NSH。
4. 保持 QSPI-XIP 的 PH2/PH3 复用保护、LAN8740A/MII/PHY=1 配置和现有
   DHCP 联通能力。
5. 状态变化必须去抖并仅在 link edge 上执行 carrier/DHCP 动作，避免
   每次轮询重复重置接口或重新申请地址。
6. 构建、补丁应用和回归检查必须可重复；不得依赖未验证的 PHY IRQ
   引脚假设。
7. 链路/DHCP 生命周期应模仿常见 Linux 发行版的受管以太网语义：
   行政启用状态与物理 carrier 分离；link down 后连接进入 disconnected，
   撤销活动 IPv4、默认路由和掩码，但保留固定 MAC 客户端身份；link up
   后重新 DHCP，服务器可继续分配原地址或按其策略返回新地址。

## Acceptance Criteria

- [x] 插线启动后 `eth0` 获得非 `0.0.0.0` DHCP 地址，显示 `RUNNING`，
  PC↔板双向 ping 成功。
- [x] 拔线后在约 2 秒目标窗口内仍显示行政 `UP`、不再显示 `RUNNING`，
  IPv4 为 `0.0.0.0` 且旧默认路由失效；既有 UI 与普通 NSH 命令仍可用。
- [x] 插回网线后无需人工命令即可重新获得有效 DHCP 地址并恢复双向
  ping。
- [x] 连续执行至少 3 次拔出/插回循环，不崩溃、不死锁、不泄漏工作项，
  QSPI-XIP/LVGL/触摸保持正常。
- [x] 静态回归能断言 link polling、carrier on/off、DHCP 重连和 B01
  QSPI 共脚保护合同。

## Out of Scope

- ESP-01、双网卡策略、MQTT 与 UI 网络状态展示
- 为所有 STM32H7 板型实现通用 PHY interrupt 框架
- 修改 LAN8740A 硬件 strap 或飞线增加中断

## Decisions

- 采用常见 Ubuntu/Debian 受管 DHCP 连接的用户可见语义，而不是仅清除
  `RUNNING`：确认 link down 后，`ifconfig` 的活动 IPv4 应为 `0.0.0.0`，
  默认路由和掩码不再作为有效网络配置；重新 link up 后自动 DHCP。
- “清除活动地址”不等于向服务器释放租约。物理链路已经断开时无法可靠
  发送 DHCP RELEASE；重连仍以固定 MAC `00:e0:de:00:a7:50` 标识客户
  端，因此在 DHCP 服务状态未改变时通常会取回原地址，但不作 IP 恒定
  保证。
- 用户确认行政状态与 carrier 必须分离：拔线时 `eth0` 保持 `UP`，只清除
  `RUNNING` 和活动 IPv4/路由；不得通过持久 `ifdown` 让设备消失。

## Technical Notes (non-binding)

- 首选约 500 ms 轮询、连续两次一致才确认 edge，目标响应约 1 秒且 CPU
  开销可控；最终参数需硬件测量。
- DHCP 重连不能只依靠当前 `netinit_monitor()`：其 link-up 分支只执行
  `SIOCSIFFLAGS`，没有重新调用 `netlib_obtain_ipv4addr()`。
- 本任务不新增持久 DHCP lease 文件；“优先拿回原地址”依赖固定 MAC 和
  DHCP 服务器仍保留租约，语义与地址是否相同分离。
