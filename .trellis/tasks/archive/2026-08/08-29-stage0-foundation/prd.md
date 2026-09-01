# 阶段 0 地基（父任务）

> 依据 `VelaGuard_推进方案.md` §5、`VelaGuard_项目手册.md`。  
> **不产生可见产品功能**，但阶段 1（运营助手 / 配置持久化 / OTA）全部依赖本阶段。

## Goal

交付四项可独立验收的地基能力，使后续阶段有可靠总线时序、可观测链路质量、可掉电保存的块存储与配置原子提交。

## Task Map

| 子任务 | 目录 | 依赖 | 优先级 |
|--------|------|------|--------|
| eMMC bring-up | `08-29-stage0-emmc` | 无 | **P0 先做** |
| RS485 时序 | `08-29-stage0-rs485-timing` | 无（可与 eMMC 并行） | P1 |
| 帧级质量统计 | `08-29-stage0-frame-stats` | 建议 RS485 时序稳定后 | P2 |
| 掉电安全存储 | `08-29-stage0-powerfail-store` | **硬依赖 eMMC** | P2 |

父任务本身不做实现；负责跨子任务验收与阶段出口。

## Cross-child Acceptance

- [x] eMMC：挂载成功，写文件 → 重启 → 读回一致（2026-08-29，含 LFN）
- [x] RS485：DIR 时序正确（2026-08-29 LA/COM6 Hex×6 `61…7A`）；应用层无 `usleep(50000)`（已归档）
- [x] 帧统计：`vgstats inject` + `vgmodbus`(MThings) + `dump`（2026-08-30；读超时 2000ms）
- [x] 掉电安全：双槽 commit/损坏恢复/出厂回退板测通过（2026-08-29）；50 次随机断电为 stretch

## Stage0 集成验收进度

见 `stage0-acceptance.md`；板端命令：`scripts/stage0_accept_nsh.txt`

| 节 | 状态 | 备注 |
|----|------|------|
| 1 eMMC | ✓ | 写读 + 冷复位（手动） |
| 2 RS485 | ✓ | `vgrs485` + 历史 Hex |
| 3 Modbus | ✓ | MThings 点表 1/2/3；8N1 + 2000ms timeout |
| 4 帧统计 | ✓ | inject + 有总线 dump |
| 5 掉电 | ✓ | `vgcfg` seq=2 复位读回；50× stretch 可选 |

**Stage0 父任务：Cross-AC 全部通过（2026-08-30）。**

## Out of Scope（父级）

- ai_agent 正式集成、Skill、LVGL、OTA、总线自动探查（阶段 1+）
- 用 microSD 替代 eMMC（仅作 eMMC 失败时的应急分支，见子任务风险）

## Ordering Rationale

优先 eMMC：阶段 1 的 `/data` 持久化与掉电安全存储硬依赖块设备；RS485 可并行但帧统计可暂基于现有 usleep 路径。

## References

- `VelaGuard_推进方案.md` §5
- `VelaGuard_项目手册.md` §3.1、§9、§16.8
- 子任务各自 `prd.md`
- **Stage0 板测 / MThings 点表**：`stage0-acceptance.md`；工程文件 `config/mthings/velaguard.mthings`（主副本 `C:\Users\19y\Documents\mthings\velaguard.mthings`）
