# Implement: VelaGuard expansion board planning docs

## Ordered checklist

1. [x] **核对焊桥默认态** — B01 PnP 已写入 docs §5 与 design.md  
2. [x] **机械接口** — PnP 记录 STMod SQT-110 RA、Arduino 2.54 座高度；画板前需实物复测相对坐标  
3. [x] **权威文档** `docs/velaguard-expansion-board.md`  
4. [x] **交叉链接** — README 进度表已链到该文档  
5. [x] **纸面验收** — 禁用 D0/D1、I2C4、PH12；RS485/ESP/DO 合同齐全  
6. [ ] **Quality check** — 用户/trellis-check 确认后可 archive  
7. [x] **不在此任务**：KiCad、投板、焊接、改固件  

## Validation

| Check | How |
|-------|-----|
| 引脚无冲突 | 对照 pinmux.csv + 本文禁用表 |
| 与 UART7 合同一致 | diff 对 `07-13` design 引脚表 |
| 焊桥可执行 | 原理图/实物核对后更新文档 |
| 文档可指导画板 | 第三方只读文档能否列出连接与 BOM |

## Rollback

- 删除或回退 `docs/velaguard-expansion-board.md` 与 README 链接。  
- 任务产物 `design.md`/`prd.md` 保留在 task 目录直至 archive。

## Risky points

- ST 焊桥丝印/编号以原理图为准，勿凭记忆焊接。  
- ESP 峰值电流导致 5V 轨跌落 → LDO 输入电容与线宽。  
- 单板同时插 Arduino+STMod 的机械干涉。
