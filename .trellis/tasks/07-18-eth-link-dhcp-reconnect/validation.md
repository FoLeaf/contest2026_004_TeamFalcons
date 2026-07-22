# Validation: B01 Ethernet carrier and DHCP reconnect

## Software gates

- Final incremental test/release build passed. The QSPI image ends at
  `0x9006c5d0`; the 720-byte boot stub ends at `0x080002d0`.
- `harness/qspi_boot_flow_check.py --artifacts .debug` passed every assertion.
- NuttX `checkpatch.sh` passed for `stm32_ethernet.c`.
- Both maintained patches pass `git apply --reverse --check` against the
  applied NuttX/apps checkouts.
- Contest AI logs pass the official validator after re-exporting the
  cross-date Codex session through the official collector.

## Upstream delivery

- NuttX driver and board changes are submitted as
  `open-vela/nuttx#323` at commit `00b9367093c`; upstream checkpatch and CLA
  verification pass.
- netinit and netlib changes are submitted as `open-vela/nuttx-apps#112` at
  commit `9c7017daf`; upstream checkpatch and CLA verification pass.
- Both commits use `Foleaf <id19y@outlook.com>` for author, committer, and
  `Signed-off-by`, matching the signed openvela CLA identity.
- Latest observed upstream state: CLA, checkpatch, setup, flagchip, and SIL
  checks pass on both PRs; Goldfish, QEMU, and AURIX matrices remain running,
  with no review comments submitted yet.

## Hardware gates

- CubeProgrammer programmed and verified the external QSPI image and internal
  boot stub, then reset the MB1381 H750XB-B01 successfully.
- Cable-present boots reached VelaGuard, LVGL framebuffer, `/dev/input0`, UI,
  and serial NSH. Final DHCP address was `192.168.137.19/24`; PC-to-board ping
  was 4/4 and board-to-PC ping was 4/4.
- Confirmed cable loss showed Windows `Disconnected / 0 bps` and board state
  `UP` without `RUNNING`, with IPv4/router/netmask all `0.0.0.0`. Offline ping
  returned network-unreachable error 101.
- Three unplug/replug cycles completed without manual `renew`. Automatic DHCP
  produced different valid addresses during the run (`.194`, `.5`, `.152`),
  demonstrating that reconnect identity is stable while address reuse is not
  guaranteed. Bidirectional ping recovered with zero packet loss.
- UI, touch initialization, storage readiness, acquisition mock backend, and
  NSH remained available throughout validation.
