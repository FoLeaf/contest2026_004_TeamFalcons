# Design: B01 Ethernet carrier and DHCP reconnect

## Boundaries

| Layer | Owns | Must not own |
|---|---|---|
| STM32H7 Ethernet driver | LAN8740A link sampling, debounce, MAC speed/duplex sync, `netdev_carrier_on/off()` | DHCP, IPv4 policy, UI state |
| netinit | Observe `IFF_RUNNING`, clear active IPv4 configuration, acquire/retry DHCP | Direct PHY register policy, QSPI pins |
| VelaGuard application | Existing UI/NSH startup only | A second competing network manager |

The driver and netinit changes are one integrated deliverable: driver-only
carrier edges leave stale IPv4, while netinit-only polling cannot truthfully
change carrier. They are therefore not split into child tasks.

## User-visible state machine

| State | `IFF_UP` | `IFF_RUNNING` | IPv4 / route | Action |
|---|---:|---:|---|---|
| Admin enabled, cable absent | 1 | 0 | `0.0.0.0`, no active default route | Poll PHY only |
| Link debounce-up | 1 | 0 | unchanged | Require two consistent samples |
| Link up, DHCP pending | 1 | 1 | `0.0.0.0` | Run/retry DHCP in netinit thread |
| Online | 1 | 1 | leased IPv4 + route | No repeated DHCP on steady polls |
| Link debounce-down | 1 | 1 | leased IPv4 | Require two consistent samples |
| Disconnected edge | 1 | 0 | clear IPv4/netmask/router | Notify clients; keep UI/NSH alive |

No DHCP RELEASE is attempted after physical loss. The stable software MAC is
the reconnect identity; the server may return the old or a new address.

## Driver design

The maintained STM32H7 patch gains a board/config-gated link polling path:

1. `ifup()` initializes MAC/PHY even when no cable is present, leaves the
   interface administratively up, publishes carrier-off, and schedules a
   delayed LPWORK item.
2. The worker reads `MII_MSR` twice per sample because BMSR link status is
   latch-low. It runs every 500 ms and requires two equal samples before an
   edge, producing an approximately 1-second debounce target.
3. On a rising edge, wait for auto-negotiation completion without blocking
   unrelated tasks, read LAN8740A PHYSR, synchronize STM32 MAC speed/duplex,
   then call `netdev_carrier_on()`.
4. On a falling edge, call `netdev_carrier_off()` once. Do not clear `IFF_UP`
   and do not reinitialize QSPI-conflicting pins.
5. `ifdown()` cancels link work and calls carrier-off before hardware reset.

Polling is selected over `CONFIG_ARCH_PHY_INTERRUPT`: the STM32H7
`stm32_phyintenable()` implementation is missing, and B01 exposes the
multiplexed `MII_TX_ER_nINT` net without a verified board interrupt GPIO.

## netinit design

Add a carrier-poll monitor mode that depends on `NETINIT_THREAD` and
`NETDEV_PHY_IOCTL`, but not `ARCH_PHY_INTERRUPT`:

1. Poll `SIOCGIFFLAGS`; never toggle `IFF_UP` for a physical link edge.
2. On `RUNNING -> !RUNNING`, set IPv4, netmask, and default router to zero.
3. On `!RUNNING -> RUNNING`, call `netlib_obtain_ipv4addr()` when DHCP is
   selected. A failed request is retried only while carrier remains present.
4. If boot starts without cable, initial ifup succeeds carrier-off and the
   persistent netinit thread waits without blocking VelaGuard or NSH.
5. If boot starts with cable, avoid a duplicate DHCP request when a valid
   address already exists.

The project build enables the polling monitor and keeps interrupt monitor
configuration disabled. Team-owned patches cover both the NuttX and apps
checkouts and remain guarded/idempotent.

## Timing and failure behavior

- PHY poll: 500 ms; confirmed edge: two equal samples; target <= 2 seconds.
- DHCP calls may use their configured timeout/retry budget, but only the
  background netinit thread waits.
- MDIO read failure does not invent a link edge. Log a rate-limited error and
  retain the last confirmed state.
- A DHCP failure leaves `UP + RUNNING + 0.0.0.0` and retries later; it must not
  reset the UI or MAC continuously.

## Compatibility and rollback

- Preserve MII, LAN8740A, PHY address 1, QSPI-XIP PH2/PH3 protection, LVGL,
  touchscreen, storage, and serial NSH.
- Gate new driver behavior to the maintained B01/QSPI build contract so other
  STM32H7 boards do not silently change.
- Roll back by disabling carrier-poll Kconfig and reverting the two maintained
  upstream patches; the previous one-shot DHCP behavior remains available.

## Validation matrix

1. Cable-present cold boot: DHCP + bidirectional ping.
2. Cable removal: `UP`, not `RUNNING`, `0.0.0.0`, no usable old route.
3. Cable reinsertion: automatic DHCP + ping without NSH command.
4. Three removal/reinsertion cycles, including a DHCP server that returns the
   same address and acceptance of a changed address.
5. Cable-absent cold boot followed by later insertion.
6. QSPI-XIP/LVGL/touch/NSH regression throughout.
