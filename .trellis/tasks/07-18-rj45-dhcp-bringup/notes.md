# Notes: H750 RJ45 DHCP bring-up

## Kconfig keys actually enabled

Contest build script (`.debug/openvela_windows_build.sh` and generator
`scripts/windows_build_openvela.ps1`) appends:

| Key | Value | Role |
|-----|-------|------|
| `CONFIG_NET` | y | stack |
| `CONFIG_NET_ETHERNET` | y | eth link |
| `CONFIG_NET_IPv4` | y | IPv4 |
| `CONFIG_NET_ARP` | y | ARP |
| `CONFIG_NET_TCP` | y | TCP |
| `CONFIG_NET_UDP` | y | UDP (DHCP) |
| `CONFIG_NET_BROADCAST` | y | receive DHCP broadcast replies |
| `CONFIG_NET_UDP_CHECKSUMS` | y | UDP CSUM |
| `CONFIG_NET_ICMP` | y | ICMP echo |
| `CONFIG_NET_ICMP_SOCKET` | y | ping socket |
| `CONFIG_NET_SOCKOPTS` | y | required by DHCPC select of BINDTODEVICE |
| `CONFIG_NET_ETH_PKTSIZE` | 1500 | MTU-ish buffer |
| `CONFIG_STM32H7_ETHMAC` | y | MAC driver |
| `CONFIG_STM32H7_MII` | y | B01 full MII bus |
| `CONFIG_STM32H7_MII_EXTCLK` | y | PHY-provided TX/RX clocks |
| `CONFIG_ETH0_PHY_LAN8740A` | y | B01 U9 PHY |
| `CONFIG_STM32H7_PHYADDR` | 1 | B01 strapped PHY address |
| `CONFIG_STM32H7_AUTONEG` | y | autoneg |
| `CONFIG_STM32H7_PHYSR` | 31 | LAN8740A status reg |
| `CONFIG_STM32H7_PHYSR_ALTCONFIG` | y | combined speed/duplex |
| `CONFIG_STM32H7_PHYSR_ALTMODE` | 0x001c | mask |
| `CONFIG_STM32H7_PHYSR_10HD` | 0x0004 | |
| `CONFIG_STM32H7_PHYSR_100HD` | 0x0008 | |
| `CONFIG_STM32H7_PHYSR_10FD` | 0x0014 | |
| `CONFIG_STM32H7_PHYSR_100FD` | 0x0018 | |
| `CONFIG_SCHED_LPWORK` | y | eth work queue |
| `CONFIG_NSH_NETINIT` | y | nsh → netinit |
| `CONFIG_NETUTILS_NETINIT` | y | netinit logic |
| `CONFIG_NETUTILS_DHCPC` | y | DHCP client lib |
| `CONFIG_NETUTILS_DHCPC_BOOTP_FLAGS` | 0x8000 | request broadcast DHCP replies |
| `CONFIG_NETINIT_DHCPC` | y | DHCP at bring-up |
| `CONFIG_NETINIT_THREAD` | y | non-blocking no-cable |
| `CONFIG_NETINIT_NOMAC` | y | no factory MAC |
| `CONFIG_NETINIT_SWMAC` | y | fixed software MAC |
| `CONFIG_NETINIT_MACADDR_2` | 0x00e0 | MAC OUI half |
| `CONFIG_NETINIT_MACADDR_1` | 0xde00a750 | MAC lower |
| `CONFIG_SYSTEM_PING` | y | NSH `ping` |
| `CONFIG_SYSTEM_DHCPC_RENEW` | y | NSH `renew` |

Implied after olddefconfig: `CONFIG_NET_BINDTODEVICE=y`, `CONFIG_NETUTILS_PING=y`.

Fixed test MAC: **00:e0:de:00:a7:50**.

## Board patch

The MB1381 H750XB-B01 schematic is the source of truth: LAN8740A, full MII,
PHY address 1. The upstream `board.h` MII pin list is correct. Two integration
fixes are still required:

1. PH2/MII_CRS and PH3/MII_COL share QSPI bank 2 IO0/IO1. QSPI-XIP builds
   skip configuring the unused full-duplex CRS/COL inputs.
2. The STM32H7 driver must call `netdev_carrier_on()` after successful ifup;
   otherwise UDP sees no `IFF_RUNNING` and DHCP `sendto()` fails with
   `EHOSTUNREACH` before a Discover reaches the wire.

- Patch: `scripts/openvela-eth-mii-stm32h750b-dk.patch`
- Apply: `scripts/apply-openvela-eth-mii-patch.sh` (hooked from build script
  after QSPI patch)

## Build validation (2026-07-18)

- `olddefconfig`: OK after adding `CONFIG_NET_SOCKOPTS`
- Full `make -j`: OK
- Image still QSPI XIP (`arm_netinitialize` @ `0x9001b754`)
- Size: text ~766 KiB, flash region 0.57% of 128 MiB QSPI
- Symbols present: `arm_netinitialize`, `stm32_eth*`, `netinit_bringup`, `dhcpc_*`

## Hardware validation status

All in-scope hardware acceptance is complete. UI “网络” may still say 离线;
real UI network-state integration remains out of scope.

## Hardware attempt (2026-07-18)

- `windows_flash_cube.ps1 -NoBuild -ValidateOnly`: passed.
  - QSPI image range: `0x90000000..0x900bbc20`
  - Internal stub range: `0x08000000..0x080002d0`
  - External loader: `MT25TL01G_STM32H750B-DISCO.stldr`
- CubeProgrammer 2.20.0 detected STM32H750B-DK at 3.27 V and successfully
  programmed + verified both the 751.03 KiB QSPI image and 720-byte internal
  boot stub, then reset the MCU.
- Windows reported the direct Ethernet interface up at 100 Mbps and Internet
  Connection Sharing running on `192.168.137.1/24`.
- A ping/neighbor sweep of `192.168.137.0/24` found no responding board and no
  neighbor with the configured `00:e0:de:00:a7:50` MAC.
- ST-LINK VCP is present as COM7, but another Windows process currently owns
  it. Opening COM7 returned `UnauthorizedAccessException`, so board-side
  `ifconfig` and Ethernet driver logs remain unavailable.

## Root cause and successful validation (2026-07-18)

- B01 schematic disproved the README-derived RMII/LAN8742A assumption:
  actual hardware is LAN8740A, full MII, PHY address 1.
- With corrected MII pins, a temporary static `192.168.137.2/24` address made
  Windows ping pass 5/5 and ARP resolve `00:e0:de:00:a7:50`, proving the MAC,
  PHY, and MII data path.
- Wireshark captured zero DHCP frames during `renew eth0`; temporary errno
  instrumentation reported `EHOSTUNREACH` (113).
- `udp_sendto_unbuffered()` requires `IFF_RUNNING`; the STM32H7 driver only
  set `IFF_UP`. Adding `netdev_carrier_on()` after successful ifup fixed the
  send path.
- Final cold reset with cable attached automatically obtained:
  - address `192.168.137.36`
  - gateway `192.168.137.1`
  - mask `255.255.255.0`
  - state `RUNNING`
- Windows → board ping: 5/5, 0% loss. Board → Windows ping: 4/4, 0% loss.
- QSPI-XIP, LVGL framebuffer/touch initialization, VelaGuard UI, storage, and
  NSH all remained operational after the MII correction.

## No-cable reset validation (2026-07-18)

- With RJ45 physically unplugged, board-to-Windows ping changed from the
  previously proven success to two `ENETUNREACH` failures, confirming the
  disconnected test state before reset.
- CubeProgrammer issued a reset without erase or reflash. Serial startup
  reached the VelaGuard boot record and NSH prompt immediately; framebuffer
  480x272, `/dev/input0` maxpoint 1, and `[velaguard] UI ready` all completed.
- After startup, `ps` returned immediately and showed `nsh_main` running,
  `velaguard_ui` ready, and `netinit` waiting asynchronously. This proves the
  no-cable network path does not block the UI or general NSH interaction.
- Known diagnostic limitation: running `ifconfig` while the cable is absent
  can block that shell command even though ordinary NSH commands remain
  usable. This is not counted as a successful no-cable network query.
- The user then removed all board power, waited, and restored it with RJ45
  still unplugged. COM7 accepted `ps` immediately: `nsh_main` was running,
  `velaguard_ui` was alive, and `netinit` was independently waiting on a
  signal. This completes the cold-power-cycle UI/NSH acceptance item.
