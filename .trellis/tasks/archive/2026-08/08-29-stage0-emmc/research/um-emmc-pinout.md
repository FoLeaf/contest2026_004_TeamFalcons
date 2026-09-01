# eMMC pinout — reviewed against local official pack (2026-08-29)

## Authority (BOUNDARY V11)

Local pack: `F:\Project\Embeded\H750B-DK\BOARD INFO\H750B-DK`  
WSL: `/mnt/f/Project/Embeded/H750B-DK/BOARD INFO/H750B-DK`

Priority: **schematic (`mb1381-h750xb-b01-schematic.pdf`) > `bsp/` > ST UM/data brief**.  
`STM32H750B-DK_*_unofficial.*` = working notes only; used for search, then cross-checked.

## Cross-check result (stage0-emmc)

| Claim in earlier web-UM notes | Official pack | Verdict |
|-------------------------------|---------------|---------|
| eMMC on **SDMMC1** | BSP `MX_MMC_SD_Init`: `Instance = SDMMC1`; unofficial MD + CSV cite schematic sheet 3 | **OK** |
| **8-bit** bus | BSP `BusWide = SDMMC_BUS_WIDE_8B` | **OK** |
| D0–D3 PC8–11, D4–D5 PB8–9, D6–D7 PC6–7, CK PC12, CMD PD2 | BSP MSP: GPIOC 6–12, GPIOD 2, GPIOB 8–9, AF12_SDIO1 | **OK** |
| No card detect | soldered U11; BSP has no CD GPIO | **OK** |
| Capacity 4 GB vs handbook 8 GB | Data brief / UM often say 4-Gbyte; chip revision may differ — **irrelevant to bring-up** | note only |
| PC11 vs QSPI_BK2_NCS conflict | unofficial CSV flags optional strap; **BSP QSPI_BK2_CS = PG6**, not PC11 | **OK for eMMC + QSPI XIP** if we follow BSP (PG6 for BK2 CS) |
| Clock edge | BSP: `SDMMC_CLOCK_EDGE_RISING`；NuttX `board.h`: `STM32_SDMMC_CLKCR_NEGEDGE` | **Bring-up risk** — card init 失败时优先对齐 BSP RISING |
| HW flow control | BSP: `ENABLE` | 不稳时对齐 BSP |

## Implementation implications

1. Prefer BSP `stm32h750b_discovery_mmc.c` MSP as pinmux template (not web tables alone).
2. Before coding, skim schematic sheet 3 for U11 nets if any solder-bridge doubt remains (esp. PC11).
3. NuttX side: still need board `stm32_sdmmc.c`; IDMA off for `MM_REGIONS=6` remains valid.
4. Clock edge / HWFC: default follow BSP if NuttX reference board differs.

## Previous gap

Planning first cited a **web-scraped UM2488 excerpt** without opening this pack. That pin table happened to match BSP, but process was wrong under V11 — corrected here.
