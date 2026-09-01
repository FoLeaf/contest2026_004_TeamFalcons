# 掉电安全配置存储

> 父任务：`08-29-stage0-foundation`  
> **依赖**：`08-29-stage0-emmc`（已通过：`/mnt/emmc` + FAT + LFN）  
> 依据：手册 §9 布局、§16.8；推进方案阶段 0「掉电安全存储」。

## Goal

在 eMMC/FAT 上实现**双槽 + CRC32 + 单调 seq** 的配置原子提交：掉电注入下配置始终可恢复；双槽皆坏时回退出厂默认并记事件。

## Confirmed Facts

| 项 | 事实 |
|----|------|
| 介质 | `/mnt/emmc`（vfat）；正式路径目标 `/data/velaguard/config/`（可用 symlink 或 mkdir 布局） |
| FAT | **无**掉电原子性 → 必须应用层双槽 |
| 手册契约 | `point_table_a.json` / `point_table_b.json`；字段含 `schema_version`、`seq`、`crc32`、`committed` |
| 写序 | 写非活动槽 → fsync → 更新 seq/校验 → fsync → committed |
| 读选 | 启动选 `seq` 最大且 CRC 通过者 |
| 代码边界 | 逻辑放选手仓 `app/velaguard/`（如 `config_store`），不塞进公共仓板级 |
| 构建 | 可继续用 `emmc` 预设验证；日后合并进 `net` |

## Requirements

- R1 库：加载 / 提交 / 回退出厂默认 API（C）
- R2 双槽文件落在 eMMC（bring-up 可用 `/mnt/emmc/velaguard/config/`）
- R3 提交路径遵守手册写序，含 `fsync`（或等价 `IOCTL`/`sync`）
- R4 启动加载：选合法最大 seq；皆坏 → 出厂默认 + 事件/日志
- R5 NSH 或 bring-up 工具：注入写中断电的**可重复**测试钩子（主机单测优先，板端抽样）
- R6 至少一种配置载荷（可先用最小 `point_table` 或 `network` stub JSON）

## Decisions（规划默认，实现前可改）

| 决策 | 选择 | 理由 |
|------|------|------|
| 首期载荷 | 最小 `point_table` stub（或单一 `vg_config`） | 先验证双槽机制，再扩 network/rules |
| 布局 | `/mnt/emmc/velaguard/config/`；阶段 1 再迁 `/data` | 不阻塞在挂载点争论 |
| CRC | CRC32 over payload（不含 crc 字段自身） | 手册；实现简单 |
| 板端断电 N | 开发期目标 **≥10** 次手工/脚本断电；正式门禁 **50** 记为 stretch | 50 次全手工成本高 |
| 主机单测 | **要**：模拟半写损坏槽 | 先保证算法正确 |

## Acceptance Criteria

- [x] **AC1** 主机单测：半写/截断/CRC 坏/双槽冲突时选槽与回退正确
- [x] **AC2** 板上提交配置 → 复位 → 读回一致
- [x] **AC3** 模拟损坏活动槽（改文件/截断）→ 启动用另一槽
- [x] **AC4** 双槽皆坏 → 出厂默认 + 可观测日志/事件
- [x] **AC5** 笔记：API、文件格式、测试步骤（task `research/`）

## Out of Scope

- eMMC 驱动 / 宽总线（已由 emmc 子任务交付）
- OTA staging、Agent `/data/agent` 全量迁移
- UI 确认流、完整 point_table 业务语义
- 自动把 `velaguard-emmc` 合并进 `velaguard-net`（可另开）

## Risks

| 风险 | 应对 |
|------|------|
| FAT 元数据损坏仍可能丢双槽 | 文档已知限制；关键配置可再加第三只读出厂镜像 |
| fsync 在 NuttX FAT 行为差异 | 提交后读回校验；单测覆盖截断 |
| 大 JSON 写窗口长 | 首期 stub 小文件；限制体积 |

## References

- `VelaGuard_项目手册.md` §9、§16.8
- `VelaGuard_推进方案.md` 阶段 0 掉电安全存储
- `.trellis/tasks/08-29-stage0-emmc/research/emmc-bringup-notes.md`
