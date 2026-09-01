# RS485 TC / DIR notes

## Acceptance evidence

**B** — peer USB-RS485 Hex `61…7A` ×N @ 9600 (LA optional).

## Board debug findings (2026-08-29)

| Check | Result |
|-------|--------|
| LA on MAX485 **DI** | Continuous correct `61…7A` |
| LA on **DIR** | High before TX; low after idle |
| Old USB-RS485 on A/B | Heavy garble / mid-frame trash |
| Replacement transceiver | Mostly clean 26-byte frames; rare single-bit errors |
| GND | Required for stable A/B; DI can look fine without it |

**Conclusion:** MCU UART + DIR path OK. Peer PHY quality dominated “software looks broken” symptoms.

## Driver fix (`stm32h7/stm32_serial.c`)

1. RS485: **no `USART_CR1_FIFOEN`** (classic TXE/TC).
2. `.txempty = up_txempty`: RS485 waits **TXE\|TC**, then force DIR=RX (thread / `tcdrain`).
3. TC ISR: clear TCIE only — **do not** drop DIR in IRQ.
4. `up_txint(true)`: clear stale `TCCF` before enabling TCIE.
5. `up_shutdown`: DIR=RX then unconfig.

## App

- `vgrs485`: `tcdrain` only — **no** `usleep(50ms)`.
- Modbus: `VG_MODBUS_RTU_GAP_US` = RTU 3.5 char gap only.

## Verify

```text
vgrs485 tx   # repeat ≥3; COM Hex = 61..7A each time
vgrs485 rx   # peer sends alphabet → PASS
```

## Upstream PR outline

- stm32h7: RS485 tcdrain waits for TC; avoid FIFO on RS485; DIR drop in thread context
