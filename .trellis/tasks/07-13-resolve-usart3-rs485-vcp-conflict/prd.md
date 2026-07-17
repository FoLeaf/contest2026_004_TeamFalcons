# Resolve USART3 RS485 and ST-LINK VCP conflict

## Goal

Move the VelaGuard RS485 transport to an electrically independent UART while
preserving the proven ST-LINK VCP console and SWD workflow. Deliver the board
and configuration foundation, wiring guidance, and a deterministic raw
bidirectional smoke test needed before later Modbus integration.

## Background and Confirmed Facts

- The target is an STM32H750B-DK, board revision MB1381 B01.
- On MB1381 B01, USART3 PB10/PB11 is shared by Arduino D1/D0 and ST-LINK VCP.
  R184 carries target PB10 to ST-LINK RX; R188 carries ST-LINK TX to target
  PB11. The existing MAX485ESA adapter also uses PB10/PB11, so its original
  wiring creates a real electrical and traffic-ownership conflict.
- The current firmware uses USART3 at 115200 8N1 for the serial console, NSH,
  startup output, and syslog. This working COM7 development/recovery path must
  remain unchanged.
- The accepted allocation moves RS485 to UART7 and leaves USART3, R184, and
  R188 untouched. UART7 TX is PB4/Arduino D10, UART7 RX is PA8/Arduino D5, and
  the direction signal is PK1/Arduino D4.
- PB4, PA8, and PK1 are 5 V-tolerant according to STM32H750XB DS12556. PB4 is
  also NJTRST, but the project's SWD-only path uses PA13/PA14 and does not
  require NJTRST.
- The original automatic-direction MAX485ESA module will be replaced with a
  3.3 V RS485 transceiver exposing DI, RO, DE, and active-low RE. DE and
  active-low RE will be tied together and driven by PK1; high selects transmit
  and low selects receive.
- The current STM32H7 serial driver supports `CONFIG_UART7_RS485` and requires
  the board to define `GPIO_UART7_RS485_DIR`. It rejects UART RXDMA or TXDMA
  combined with RS485, so UART7 DMA must remain disabled.
- No real Modbus sensor or PC-side USB-RS485 adapter is currently available.
  Procurement is approved for the 3.3 V transceiver, a USB-RS485 adapter,
  wiring, and 120-ohm termination.
- The existing NuttX `serialrx` and `serialblaster` applications are sufficient
  for the board side of a deterministic raw-link test. A Windows PowerShell
  peer script will send and verify the matching byte sequences over the
  PC-side USB-RS485 COM port.
- The existing QSPI patch is already applied in the working NuttX checkout.
  UART7 RS485 support must use a separate idempotent patch so QSPI patch
  detection is not broken by partially applied combined content.

## Requirements

- Keep USART3 PB10/PB11 exclusively owned by ST-LINK VCP for COM7, NSH,
  startup output, and syslog at 115200 8N1.
- Assign UART7 exclusively to RS485 using PB4/D10 as TX/DI, PA8/D5 as RX/RO,
  and PK1/D4 as the active-high transmit direction output.
- Use a 3.3 V manual-direction RS485 transceiver with DE and active-low RE tied
  together. PK1 must initialize low so the transceiver starts in receive mode.
- Enable UART7 serial and RS485 support at 9600 8N1 with both UART7 DMA paths
  disabled. Keep USART3 as the serial console.
- Keep the normal production image free of bench-only smoke utilities. Provide
  an explicit test-build mode that enables `serialrx` and `serialblaster`.
- Expose `/dev/rs485` as the stable RS485 device-path contract for future
  VelaGuard code. Implement it as a board-created pseudo-filesystem link to
  the build-verified UART7 node, currently `/dev/ttyS1`, and document that
  internal numbering dependency.
- Add a Windows PowerShell peer script for deterministic PC-to-board and
  board-to-PC validation through a user-selected USB-RS485 COM port.
- Document wiring, common-ground requirements, A/B polarity handling,
  point-to-point termination, and the boundary between software-only and
  hardware acceptance.
- Preserve QSPI boot, display, touch, storage, COM7 console, and attach-only
  SWD debugging behavior.

## Acceptance Criteria

- [x] The conflict is traced to MB1381 B01 USART3 PB10/PB11 being shared by
      Arduino D1/D0, ST-LINK VCP, and the original MAX485ESA adapter.
- [x] The accepted ownership model leaves USART3 on ST-LINK VCP and assigns
      UART7 PB4/PA8 plus PK1 direction exclusively to RS485.
- [ ] A separate idempotent NuttX patch defines the UART7 direction GPIO and
      preserves the existing QSPI patch/apply workflow.
- [ ] A production build proves USART3 remains the 115200 8N1 console and
      UART7 is a non-DMA 9600 8N1 RS485 serial device.
- [ ] A test build additionally provides `serialrx` and `serialblaster`, while
      the production build omits them.
- [ ] `/dev/rs485` exists at runtime and resolves to UART7. The build verifies
      that its internal link target is `/dev/ttyS1`, and future UART enablement
      cannot silently redirect the public VelaGuard contract to another
      peripheral.
- [ ] The Windows peer script and board commands define a 26-byte test in both
      directions and reject wrong length or content.
- [ ] Documentation distinguishes software/build verification from pending
      hardware validation and includes wiring, common ground, A/B swap
      diagnosis, and 120-ohm end termination guidance.
- [ ] When the purchased hardware is available, the bench test passes in both
      directions, UART7 direction returns to receive after transmit complete,
      COM7 remains usable, and SWD attach remains functional.
- [ ] No firmware implementation, build, flash, or hardware rewiring occurs
      until `prd.md`, `design.md`, and `implement.md` are reviewed and approved.

## Out of Scope

- nanoMODBUS integration, sensor registry, periodic acquisition, alarms, UI,
  and real sensor register maps.
- Treating the future Modbus profile (slave 1, FC03, holding register 40001,
  one to three registers every one to two seconds) as hardware acceptance for
  this foundation task.
- Removing R184/R188 or moving the console away from the on-board ST-LINK VCP.
- Claiming electrical timing, frame integrity, or peer communication success
  before the approved hardware is available and tested.

## Product Hardware Tier (decided)

Accepted: **contest-demo / near-field engineering prototype**.

Implications:
- Plan isolatable RS485 protection, real Modbus sensors, and reliable power.
- Defer full EMC / industrial certification and certification-grade BOM.
- Whole-board expansion should support a credible field-like demo without
  over-claiming industrial readiness.

## Whole-Board Resource Map (approved)

Design part 1 approved as the VelaGuard expansion planning baseline. Aligned
with `VelaGuard_项目手册.md` / scratch PRD network rules (RJ45 primary,
ESP-01 backup, exclusive UART, independent 3.3 V):

| Resource | Use | Notes |
|----------|-----|-------|
| Arduino UART7 PB4/PA8 + PK1 | RS485 | Confirmed; exclusive to this foundation |
| STMod+ USART2 PD5/PD6 | ESP-01S backup Wi-Fi | Reserved exclusive UART; not this task |
| PH10 | ESP-01 reset | Reserved hardware recovery; not this task |
| ESP-01 power | Independent stable 3.3 V | Peak-current sized; not shared noisy rail |
| On-board RJ45 | Primary network | Preferred over ESP for demo reliability |
| On-board WM8994 / MEMS mic | Audio | Keep existing I2C4 consumers |
| On-board eMMC / QSPI | Logs, config, OTA | Preserve existing QSPI patch |
| On-board FDCAN | Reserved | No early claim |
| I2C4 add-ons | Avoid early | Touch + audio already share I2C4 |

This task implements only the UART7 RS485 foundation. ESP-01 / STMod+ /
network manager remain reserved ownership, not deliverables here.

## First-BOM RS485 Protection (decided)

- **Not isolated** for this foundation task.
- **TVS only:** SM712 (or equivalent dual RS485 A/B TVS) on the bus side.
- Isolation remains a later pin-compatible upgrade path and is out of scope
  for first acceptance; do not change UART7 pin map (PB4/PA8/PK1) for that
  upgrade.
- Fail-safe bias / full industrial protection still deferred unless later
  required for a field-demo upgrade.

## Transceiver Power and Ground (decided)

- Transceiver VCC from board Arduino **3.3 V**.
- Transceiver GND tied to board GND.
- USB-RS485 peer and board share a **common ground** for this non-isolated
  first BOM.

## First-Acceptance Topology (decided)

- Short **point-to-point** only: board UART7-RS485 ↔ PC USB-RS485.
- **120 Ω termination at both ends**.
- Multi-drop and real Modbus sensors are later work, not this task's
  hardware acceptance.

## How Acceptance Works (two gates)

Hardware questions above only constrain **wiring + first BOM**. They are
verified as follows — no full industrial lab:

**Gate A — software / build (no RS485 hardware required)**
- Production build: USART3 remains 115200 console; UART7 is non-DMA 9600
  RS485; `/dev/rs485` → build-verified UART7 `/dev/ttyS1`; QSPI patch
  workflow still works; production image omits bench-only tools.
- Test build: `serialrx` + `serialblaster` present; PowerShell peer script
  exists and encodes the same 26-byte bidirectional contract.
- Docs list pin map, common ground, SM712, A/B swap diagnosis, termination.

**Gate B — hardware bench (only after parts arrive)**
- Wiring checklist: PB4→DI, PA8←RO, PK1→DE+/RE, 3.3V/GND, A/B + SM712,
  both-end 120 Ω, common ground with USB-RS485 peer.
- Deterministic raw test both directions (26-byte fixed payload); reject
  wrong length/content.
- After TX, direction returns to receive; COM7 NSH still usable; SWD attach
  still works.
- Success here does **not** claim Modbus, multi-drop, isolation, or EMC.

## Delivery Phasing (decided)

- **Gate A first:** software/build/docs may complete and be treated as the
  unblocked deliverable before hardware arrives.
- **Gate B remains open:** transceiver + SM712 + USB-RS485 + 120 Ω bench
  checklist stays blocked until parts are available; do not claim electrical
  success until Gate B passes.
- Task archive policy: may archive only after Gate A is done **and** Gate B is
  either passed or explicitly deferred to a follow-up task with the remaining
  checklist copied forward (decide at finish time, not now).

## Finish Policy When Gate B Is Blocked (decided)

- After Gate A is complete, create a small **follow-up task** for Gate B
  hardware bench acceptance and archive this foundation task.
- Copy any remaining Gate B checklist into the follow-up task.
- Do not block later Modbus planning on RS485 hardware logistics.

## Documentation Home (decided)

- Authoritative wiring + Gate A/B acceptance doc:
  `docs/velaguard-rs485-uart7.md`
- README only links that file; do not duplicate the full pin/BOM checklist
  into README body or task artifacts as the sole home.
