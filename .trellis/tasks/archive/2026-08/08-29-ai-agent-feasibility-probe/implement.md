# 实施计划：ai_agent H750 探针

> **先 `task.py start` 再写代码**（用户确认本计划后）。
> 工时硬上限 **20h**；每完成一 `[required]` 步记录耗时。

## Phase 0 — 准备

- [x] **0.1** 创建 `research/probe-log.md` 记录起始时间与环境（WSL、工具链、openvela 分支）
- [x] **0.2** 阅读 `packages/ai_agent/defconfigs/esp32s3-eye/` 与 Makefile 中 voice/ 编译条件
- [ ] **0.3** 确认 `velaguard-net` 预设仍 `bash scripts/build.sh net` 通过（网络基线未坏）

## Phase 1 — defconfig 与构建（AC1）

- [x] **1.1** 新增 `velaguard-ai-probe` defconfig patch（基于 net + §4.4 Kconfig）
- [x] **1.2** `scripts/build.sh` 增加 `ai-probe` TARGET（或等价文档化入口）
- [x] **1.3** 首次 `configure + make`，收集链接错误清单
- [x] **1.4** 按需添加 fix（`CONFIG_SYSTEM_SYSTEM=y`；暂无独立 fix 脚本）
- [x] **1.5** **AC1 通过**：全量编译链接无 error

```bash
cd contest2026_004_TeamFalcons
bash scripts/build.sh ai-probe   # 或文档中的 configure 命令
```

## Phase 2 — 板端启动（AC2）

- [x] **2.1** 烧录 `.debug/nuttx.hex` + bootstub
- [x] **2.2** NSH 启动，确认 littlefs/RAMMTD 挂载（若 ai_agent 依赖 `/data`）
- [x] **2.3** `nsh> ai_agent` → 出现 `vela>` 提示符
- [x] **2.4** **AC2 通过**

## Phase 3 — 网络（AC3）

- [x] **3.1** 确认 RJ45 DHCP（与 vgmqtt 相同网络环境）
- [x] **3.2** `vela> net_test` 通过
- [x] **3.3** **AC3 通过**

## Phase 4 — LLM（AC4）

- [x] **4.1** 本地配置 `agent_secrets.h`（不提交）— 改用 CLI `set_llm`（tmpfs）
- [x] **4.2** `vela> ask 你好` 收到非空回复
- [x] **4.3** **AC4 通过**

## Phase 5 — 收尾

- [x] **5.1** 编写 `research/probe-report.md`（AC5）
- [ ] **5.2** 更新 `docs/velaguard-bringup-known-issues.md` 探针结论摘要
- [ ] **5.3** `trellis-check`（若触及 contest 仓代码）
- [ ] **5.4** 探针通过 → 提示用户开「阶段 1 ai_agent 正式集成」任务；失败 → 写决策建议

## 回滚

- defconfig patch 搞坏 net/min：单独 revert ai-probe patch，不影响现有 TARGET
- 不在探针任务内改 velaguard 业务逻辑

## start 前检查

- [ ] prd.md / design.md / implement.md 已审
- [ ] 用户确认可以开始实现（`task.py start`）
