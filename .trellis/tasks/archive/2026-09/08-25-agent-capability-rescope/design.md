# 设计：Agent 运营助手场景（v3）

> 本文是写入 `VelaGuard_项目手册.md` 与 `VelaGuard_推进方案.md` 的实质草稿。

---

## 1. 新的分工口径

**确定性逻辑负责「采、算、判」；Agent 负责「讲」。**

| 问题 | 由谁做 |
|---|---|
| 从站扫描、字序求解、周期采集 | 确定性算法 |
| 阈值 / 离线 / 链路劣化告警 | 规则引擎（本地，断网照常） |
| 485 故障归因 | 规则库（阶段 2，确定性） |
| **这条告警对人意味着什么？该关注什么？** | **Agent（告警解释）** |
| **这段时间运行概况如何？** | **Agent（日报 / 周报）** |
| **「3 号从站温度多少？」** | **Agent + 只读查询工具（交互）** |

Agent **不**设计总线实验、**不**写寄存器、**不**自动改配置。

---

## 2. Agent 工具集（全部只读）

| 工具 | 作用 |
|---|---|
| `get_live_values(slave, tags[])` | 当前采样值 |
| `get_alarm_context(alarm_id)` | 告警实例、触发规则、关联点位 |
| `get_events(since, until, filter)` | 结构化事件 |
| `get_telemetry_summary(window, slaves[])` | 窗口内 min/max/avg、离线时长 |
| `get_frame_stats(slave, window)` | 通信质量摘要（可选） |
| `get_point_table()` | 点名、单位、阈值 |

**移除**：`modbus_scan`、`set_probe_baudrate`、故障探测类工具。

沙箱：参数硬上界、限流、会话门控（告警解释 / 报告生成 / 交互查询三类会话）、全量审计日志。

---

## 3. 场景 A：告警解释（事件主动，赛道主证据之一）

```text
规则引擎产生告警（阈值 / 离线 / 链路劣化 …）
→ 本地告警与 UI 展示（不依赖网络）
→ 若网络可用：自动启动「告警解释会话」
→ Agent 调用 get_alarm_context / get_live_values / get_events / get_telemetry_summary
→ 输出结构化解释（summary、evidence、related_points、suggested_attention）
→ LVGL 告警详情页展示（标注「AI 推测」）+ MQTT alarm 附解释摘要
→ 不自动清除告警、不改阈值、不写寄存器
```

**Skill：`alarm_interpretation.md`**

- 如何组织证据（当前值、近期趋势、同从站其他点、近期事件）
- 如何把规则引擎的「发生了什么」翻译成现场人员能懂的话
- 何时承认信息不足（`unresolved: true`）
- 不得建议在线写寄存器或自动处置

**输出 schema：**

```json
{
  "alarm_id": "",
  "summary": "",
  "evidence": [],
  "related_points": [],
  "suggested_attention": [],
  "source": "agent",
  "unresolved": false
}
```

---

## 4. 场景 B：日报 / 周报（定时主动，赛道主证据之二）

```text
定时器到点（如每日 08:00 / 每周一 08:00，可配置）
→ Agent 调用 get_events / get_telemetry_summary / get_alarm_context（汇总未恢复告警）
→ 生成 Markdown 或 JSON 报告
→ 写入 /data/velaguard/reports/
→ LVGL「报告」页预览 + 可选 MQTT 上报 + 事件日志记录
```

**Skill：`operations_report.md`**

- 日报 / 周报结构（告警摘要、关键指标、通信质量、 notable 事件）
- 指标口径（与 event_store 字段一致）
- 异常写法（不夸大、不编造未采集数据）

**输出 schema：**

```json
{
  "report_type": "daily|weekly",
  "period_start_ms": 0,
  "period_end_ms": 0,
  "sections": [],
  "source": "agent"
}
```

9/20 子集：**日报必做，周报可后补。**

---

## 5. 场景 C：自然语言查实时数据（交互，非主证据）

```text
用户（CLI vela> 或 LVGL 输入框）：「5 号从站流量怎么样？」
→ Agent 解析意图 → get_live_values / get_point_table
→ 自然语言回答，附原始数值与单位
```

满足「至少一个交互渠道」；**赛道书面说明的主场景仍写 A + B**，避免被判纯聊天。

---

## 6. 断网降级

| 能力 | 断网行为 |
|---|---|
| 采集 / 告警 / 统计 / 日志 | 照常 |
| 告警解释 | 不可用；告警详情展示规则引擎原始信息 |
| 日报 / 周报 | 可基于本地 events 生成；若会话需 LLM 则标记 pending，网络恢复后补跑 |
| 自然语言查数 | 可回答纯本地缓存值；需 LLM 润色时降级为结构化数值展示 |

---

## 7. 可证伪验收判据（替换手册 §14.2）

> 在 N 次人工注入的告警（覆盖超阈值、离线至少 2 类）中，记录 Agent 告警解释被人工判定「现象描述正确且建议关注项合理」的比例；另在 M 份自动生成的日报中，记录「关键指标与 events.jsonl 一致且无编造点位」的比例。全过程写操作触发次数 = 0。

目标草案：**告警解释 ≥ 70% 合理；日报 ≥ 90% 指标一致；写操作 = 0。**

---

## 8. 大赛对照

| 官方要求 | v3 落点 |
|---|---|
| ≥1 Skill | `alarm_interpretation.md` + `operations_report.md` |
| ≥1 主动+执行 | **事件主动**（告警解释）+ **定时主动**（日报） |
| 交互渠道 | CLI + LVGL；自然语言查数 |
| 非纯聊天 | 工具调用 + 事件/定时触发 |

---

## 9. 原排障能力的去向

- §5.7 九条决策表 → 阶段 2 规则库，**非 Agent**
- 帧统计 → 保留，服务告警与规则库
- 「Agent 自主探测实验」→ 移除，理由写入推进方案 §2.6
