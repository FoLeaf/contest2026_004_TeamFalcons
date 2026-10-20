# 板端网络故障现场（RSP-01 验收收尾第 1 项）

日期：2026-10-20 13:38-14:40。只读检查，未在归因前做任何恢复动作。

## 现象

失败路径测试完成后，板端请求开始快速失败：

```
[vela_tls] ssl_handshake ret=-0x004e: NET - Sending information through the socket failed
[vela_tls] net_connect token-plan-cn.xiaomimimo.com:443 ret=0x52
http ... dns_tcp=na(fused) ... rc=-1 status=0   http=90100
```

`ret=0x52` 是 `MBEDTLS_ERR_NET_SEND_FAILED`。

## 只读检查结果

### 板端

```
vgnet status
state=down egress=none tcp=none mqtt=down reason=egress=none
rj45 link=1 ip=192.168.137.64 gw=192.168.137.1 ping_ok=0 fail_streak=10

ifconfig
eth0  Link encap:Ethernet HWaddr 06:01:f5:3c:d6:7a at RUNNING mtu 1486
      inet addr:192.168.137.64 DRaddr:192.168.137.1 Mask:255.255.255.0

arp -a 192.168.137.1
HWaddr: 54:6c:50:33:4b:81
```

- 链路 `RUNNING`，IP 与掩码正确，网关与 IP 同网段
- ARP 已解析到网关 MAC，二层可达
- `ping_ok=0`、`fail_streak=10`，ping 十包全丢
- MTU 1486，比标准的 1500 小 14 字节

### 主机侧

```
Get-NetIPAddress 192.168.137.1  -> InterfaceIndex 7, 别名 以太网
Get-NetAdapter -InterfaceIndex 7
  Name: 以太网  Status: Up  LinkSpeed: 100 Mbps
  InterfaceDescription: USB2.0 Ethernet Adapter
SharedAccess (ICS)  Status: Running
默认路由: ifIndex 21 以太网 3  10.16.0.1   metric 0
防火墙: Domain/Private/Public 三个配置全部 Enabled=False
```

ICS 共享的目标是板子直连的那块 USB 网卡，默认路由走另一块千兆网卡。
配置本身正确，防火墙未参与。

### 主机到板方向

```
Test-Connection 192.168.137.1  -> True
Test-Connection 192.168.137.64 -> True
Get-NetNeighbor 192.168.137.64
  06-01-F5-3C-D6-7A  Probe
  00-00-00-00-00-00  Unreachable
```

Windows 能 ping 通板子。ARP 表里板子条目同时存在 Probe 与 Unreachable
两条，说明邻居表在反复重新学习。

## 已排除与未排除

| 判断 | 依据 |
|---|---|
| 不是固件改动导致 | 同一固件此前请求正常；失败出现在网络环境变化之后 |
| 不是防火墙 | 三个防火墙配置全部关闭 |
| 不是 ICS 未运行 | `SharedAccess` 为 Running |
| 不是 IP 或掩码错误 | 板端与主机侧读数一致，同网段 |
| 不是二层不通 | 板端 ARP 到网关，主机 ping 通板子 |
| **未排除**：ICMP 转发或 NAT | 板端 ping 网关无响应，但主机能 ping 通板子，呈单向可达 |
| **未排除**：MTU 与分片 | 板端 MTU 1486，TLS 握手包可能受路径 MTU 影响 |

## 板端 ping 的警告不代表丢包

日志中反复出现：

```
WARNING: Ignoring ICMP reply with ID N.  Expected 26
```

来源是 `apps/system/ping/ping.c:182` 的 `ICMP_W_IDDIFF` 分支：板子**收到了
ICMP 回复**，但回复的 ID 与本次 ping 会话不符，被工具丢弃。NuttX 用进程
ID 作为 ICMP 标识，多个 ping 会话或复用导致 ID 对不上。这条警告本身不能
作为"网络不通"的证据，只能说明 ping 工具丢弃了确实到达的包。

## 定位结果：ICS 的 DHCP 未提供租约

继续排查后找到直接原因。复位重新烧录一次后，板端读数变为：

```
vgnet status
state=down egress=none tcp=none mqtt=down reason=boot
rj45 link=1 ip=- gw=223.5.5.5 ping_ok=0 fail_streak=0

ifconfig
eth0  Link encap:Ethernet HWaddr 06:01:f5:3c:d6:7a at RUNNING mtu 1486
      inet addr:0.0.0.0 DRaddr:0.0.0.0 Mask:0.0.0.0

[netmgr] Timed out waiting for network
[agent] Network timeout — will retry until link is up.
```

板端 IP 为 `0.0.0.0`，DHCP 没有拿到租约。主机侧对应读数：

```
Get-Service SharedAccess           -> Status: Running
Get-NetUDPEndpoint -LocalPort 67   -> 无任何监听
```

`SharedAccess` 服务在运行，但 **UDP 67 没有任何进程监听**，也就是 ICS
的 DHCP 组件没有在工作。板子因此拿不到地址；此前一度拿到的
`192.168.137.64` 在租约到期后无法续租，于是退回 `0.0.0.0`。

这解释了本次全部现象，也解释了为什么两次采集之间板端 IP 会变化。

### 与前一项检查的关系

上一层检查里"主机能 ping 通板子"成立，是因为那时租约还在、地址未过期。
链路层（ARP、RUNNING、速率）自始至终正常，问题不在线缆、不在交换机，
也不在固件的网络驱动。

## 本次未继续深挖的部分

`SharedAccess` Running 但 DHCP 不监听，属于 Windows ICS 组件状态异常，
常见于共享配置在网卡切换后没有重新应用。恢复手段是重新启用一次共享
或重启 ICS 服务；这两者都会改动主机网络配置，超出"只读检查"的范围。

本轮没有执行恢复动作，也没有声称网络已恢复。后续用主机可控服务完成的
项目不依赖板端出网，因此不受影响。
