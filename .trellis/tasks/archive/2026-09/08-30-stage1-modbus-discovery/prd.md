# Modbus 总线自动探查

> 父任务：`08-30-stage1-min-product`  
> **依赖**：stage0 RS485 DIR/TC、`vg_frame_stats`、`vg_config_store`（`/data/velaguard/config`）  
> **下游**：`08-30-stage1-lvgl-hmi`（点表预览/确认 UI）  
> **口径**：手册 §5.1；Agent **不参与**扫描（`08-25-agent-capability-rescope/design.md` §2）

## Goal

交付大赛 9/20 子集「Modbus 自动扫描 + 点表生成」的**确定性**最小闭环：在 **固定 9600 波特率** 下发现 RS485 存活从站地址，探测可读寄存器块，生成候选点表 JSON，经 NSH 测试读取后可 `vgcfg commit` 落盘。

## Requirements

- R1 **地址扫描（9600 固定）**：波特率固定 **9600**（与 MThings mock / 扩展板从站一致；Kconfig `VG_DISCOVER_BAUD` 默认 9600，仅 bring-up 可改）；地址范围默认 1–32（可配至 247）；每地址发最小 FC03/FC04 试探；遵守 3.5 字符帧间隔；结果写入结构化 scan 摘要
- R2 **寄存器块探测**：对每个存活从站，按步进块试读 Holding/Input；记录可读区间与 Modbus 异常码；不打满总线（块间 inter-frame delay）
- R3 **字序/类型初判（MVP）**：对连续 2/4 寄存器尝试 ABCD/BADC/CDAB/DCBA 解码 float32；对单寄存器尝试 int16/uint16 + 常见 scale `{1,0.1,0.01}`；输出**候选项**而非唯一断言
- R4 **点表生成**：探测结果 → `point_table_candidate.json`（schema v1）；字段含 slave、baud（恒 9600）、fc、addr、qty、dtype、word_order、scale、unit、tag；与现有 `vg_config_store` slot 文件分离，确认后再 merge/commit
- R5 **NSH 工具**：`vgdiscover` 子命令：`scan`、`probe`、`dump`、`test-read`、`apply`（apply 仅写 inactive slot + 需显式 confirm 标志）
- R6 **与采集衔接**：`apply` 后 `modbus_collector` / 周期采集可读新点表（本任务可先 stub 加载路径，完整热加载可与 LVGL 并行）
- R7 `bash scripts/build.sh net` 编译链接通过；host 侧纯逻辑单元测试（scan 状态机、JSON 序列化、字序解码）

## 刻意排除

- **波特率矩阵扫描**（19200/38400/…）— 阶段 1 固定 9600；多波特率留阶段 2 或现场手工配置
- Agent 工具 `modbus_scan` / 自然语言创作点表
- LVGL 扫描进度与点表预览（`stage1-lvgl-hmi`）；**HMI 扫描开关默认关**，须用户显式开启
- 写寄存器、改从站参数
- 完整约束求解器 / ML 推断（阶段 1 用启发式 + 人工确认）
- MQTT 推送扫描结果
- 上电 / 进入 HMI 自动扫描（改为 **UI 开关默认关**，见手册 §5.1）

## Acceptance Criteria

- [x] **AC1** net 预设编译链接通过
- [x] **AC2** NSH：`vgdiscover scan` @9600 在接 mock 从站时列出 ≥1 存活地址（**Modbus Slave**；板端 COM3）— 实测 **32/32**
- [x] **AC3** NSH：`vgdiscover probe -a 1` 输出 ≥1 可读 holding 块（起止地址 + 样例 raw 值）
- [x] **AC4** `vgdiscover dump` 生成 `point_table_candidate.json`；含手册要求字段；对 mock 温湿度从站推断温度/湿度条目
- [x] **AC5** `vgdiscover test-read` 对候选点实读，打印解码值；`vgdiscover apply --confirm` 写入 config slot 且 `vgcfg dump` / `points.json` 可读
- [x] **AC6** host_tests 新增用例通过；笔记落盘 `research/modbus-discovery-notes.md`

## References

- `VelaGuard_项目手册.md` §5.1、§5.9、§14.1
- `VelaGuard_推进方案.md` §6.1
- `.trellis/spec/backend/vg-config-store.md`
- `config/mthings/velaguard_sensors.csv`（期望探测对齐的 mock 点表，9600）
- `app/velaguard/modbus_collector.c`、`vg_modbus_read.c`（复用 transport）
