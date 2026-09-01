# 设计：阶段 1 剩余闭环（父任务）

> 只覆盖 **尚未过出口** 的集成。已归档子任务的设计不在此重复。

## 1. 演示固件边界

| 预设 | 用途 |
|------|------|
| `velaguard-net` | 日常无 LVGL：Agent 日报、`vgdiscover`、告警线程 |
| `velaguard-lvgl` | 9/20 演示：LTDC HMI + net 能力；contest SoT：`scripts/configs/velaguard-lvgl.defconfig` |

INIT 均为 `velaguard_app_main`。HMI：`VG_HMI_AUTOSTART` 等 `/dev/fb0` 后 `task_create(vghmi)`。

SRAM ~92.5% 时必须 `BOARD_SDRAM2_HEAP_OFFSET=1MB`，否则 SDRAM 堆与 LTDC FB（`0xD0000000`）重叠。该改动在 nuttx `stm32_allocateheap.c` / `board.h`（与 `velaguard/display-acceleration` 同源）。

## 2. 剩余数据流（HMI AC4–AC7）

```text
探查开关 OFF（默认）
  → 不调用 vg_bus_scan
  → 首页：get_slaves / points.json / 上次 discover 缓存；无则空态

探查开关 ON
  → backend discover_scan_start（已有 pthread + vg_bus_scan）
  → 列表 get_slaves → vg_model_import_discover_slaves
  → 屏上 confirm → vg_config_commit / points.json（须新接，禁止写寄存器）

报告页
  → read_latest_report 已读 /data/agent/reports/daily-*.md
  → 空态文案已有；板测需 eMMC 上确有日报或确认 empty

告警页
  → 「AI 推测」样式已有；真实字段读 pending_alarm / agent 落盘，断网用规则原文
```

首页当前：板端 `seed_fleet()` 空，直到 discover import。AC4 可用「探查后再显示」满足，不必先接 collector。

## 3. Agent 共存（Q1=B，已定）

HMI autostart 后立刻 `ai_agent` autostart 会 `_assert` 拖死整机。当前 HMI 固件 **skip agent autostart**。

合同：

1. HMI AC4–AC7 **不以**同机 Agent 为前置
2. 日报 / CLI 查数用 `velaguard-net`
3. D1（HMI 存活后 `ai_agent &` 不 panic）尽力；失败则双固件演示并写笔记
4. 恢复 `VG_AGENT_AUTOSTART` 不是阶段 1 出口条件

## 4. CLI 查数

不新写 NSH 命令。交互渠道是已有 `nsh> ai_agent` → `vela> ask …`，Skill `modbus_query.md` + 白名单 `vgmodbus`/`vgstats`。

剩余只是 **专项板测句**：例如「1 号从站温度多少？」须看到工具调用与数值。可在 `net` 上做。

## 5. 回滚

- 演示失败：烧回 `velaguard-net`（无 LVGL）保留 Agent/探查 NSH
- HMI：`CONFIG_VG_HMI=n` 或不烧 lvgl 预设
