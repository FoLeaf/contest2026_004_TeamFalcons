# Implement: B01 Ethernet carrier and DHCP reconnect

## Checklist

1. [x] Extend the maintained STM32H7 MII patch with board-gated delayed
   link polling, double-read BMSR sampling, two-sample debounce, carrier-off
   on loss/ifdown, and carrier-on only after negotiated speed/duplex sync.
2. [x] Make administrative `ifup()` succeed without a cable while keeping
   `IFF_RUNNING` clear; ensure the link worker is canceled safely on ifdown
   and cannot queue duplicate work.
3. [x] Add an idempotent team-owned apps/netinit patch for carrier polling,
   active IPv4/router/netmask clearing, DHCP on rising edge, and bounded retry
   while carrier remains up.
4. [x] Update `windows_build_openvela.ps1` to apply both patches and enable
   only the polling monitor Kconfig; retain B01 MII/QSPI/DHCP configuration.
5. [x] Extend `harness/qspi_boot_flow_check.py` with non-tautological checks
   for BMSR double-read, edge debounce, both carrier calls, no physical-edge
   `IFF_UP` clearing, IPv4/route cleanup, DHCP reconnect, and patch idempotence.
6. [x] Build firmware, inspect final Kconfig/ELF/HEX ranges, flash and capture
   COM7 startup with cable both present and absent.
7. [x] Perform at least three unplug/replug cycles. Record transition latency,
   `ifconfig`, automatic DHCP result, and bidirectional ping each cycle.
8. [x] Re-run QSPI-XIP, LVGL, touch, UI, storage, NSH, syntax, patch reverse
   checks, and contest log validation.
9. [x] Update the STM32H750 toolchain spec with the final carrier/DHCP
   contract, then prepare a scoped commit including AI Coding logs.

## Validation commands

```bash
bash -n scripts/apply-openvela-eth-mii-patch.sh
python3 harness/qspi_boot_flow_check.py --artifacts .debug
git -C ../nuttx apply --reverse --check \
  "$PWD/scripts/openvela-eth-mii-stm32h750b-dk.patch"
git -C ../apps apply --reverse --check <maintained-netinit-patch>
python3 ../.claude/skills/contest-log-collector/tools/validate-log.py logs
```

Hardware acceptance uses COM7 `ifconfig` plus PC/board ping before removal,
after removal, and after automatic recovery. Do not use manual `renew` during
the reconnect acceptance path.

## Risky files and rollback points

| Area | Risk | Rollback |
|---|---|---|
| STM32H7 PHY/MAC worker | work-queue race, stale speed/duplex, MDIO failure | disable board-gated polling patch |
| apps netinit monitor | duplicate DHCP, stale route, blocking retry | disable poll-monitor Kconfig/apps patch |
| Windows build generator | patch/config drift | reverse-check both patches before configure |
| QSPI-XIP board pins | PH2/PH3 remux breaks execution | retain harness assertion and existing guard |

## Review gate before start

- Confirm Linux-like visible semantics: `UP` persists, `RUNNING` follows
  carrier, active IPv4/route clear on loss, DHCP recovers automatically.
- Confirm polling is acceptable because B01 PHY IRQ wiring/driver support is
  not verified.
- User must approve these artifacts before `task.py start` and implementation.
