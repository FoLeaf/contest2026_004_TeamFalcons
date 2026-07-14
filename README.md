# VelaGuard

## 一、作品简介

VelaGuard 是面向 **AI 硬件产品创新** 赛道的工业边缘 AI-Agent 网关原型。项目基于 openvela 与 STM32H750B-DK，目标是在现场设备侧完成传感器采集、本地异常告警、LVGL 人机界面，并通过 **MQTT / AI Bridge** 协作完成诊断建议与配置辅助，同时保证离线时本地安全闭环仍可用。

当前仓库在干净重启基线上推进产品实现：已有可构建的 `app/velaguard_app`（Issue #01 骨架：LVGL 首页、Device ID、`/data/velaguard` 启动目录与启动日志事件），以及 **H750B-DK Arduino + STMod+ 扩展板硬件合同**（RS485 / ESP-01 / DO）。后续 issue 将接入 Modbus、Local Safety Loop、双模网络、MQTT AI Bridge、音频与 OTA。

完整产品规格见 [`VelaGuard_项目手册.md`](VelaGuard_项目手册.md)；推进节奏见 [`VelaGuard_推进方案.md`](VelaGuard_推进方案.md)。

## 二、选题方向

**AI 硬件产品创新**。

聚焦工业现场小型设备运维（泵、风机、压缩机、电机等）：温度、振动、电流、压力或通信异常时，网关先用本地规则保证离线可用，再把事件上下文交给 AI-Agent 生成可解释的诊断建议。

边界原则：

- AI 可辅助配置、解释与诊断，但不能绕过本地确认直接控制设备。
- 网络或云侧不可用时，本地采集、告警与日志仍必须继续工作。

## 三、硬件与扩展板（摘要）

| 资源 | 用途 |
|------|------|
| STM32H750B-DK | 主控：openvela、LVGL、RJ45、ST-LINK VCP 调试 |
| 扩展板（规划已确认） | 单板对接 **Arduino + STMod+** |
| RS485 / Modbus RTU | UART7：`D10` TX、`D5` RX、`D4` DIR（**不占用** D0/D1 VCP） |
| ESP-01 Wi-Fi（备用） | STMod USART2（`PD5`/`PD6`）+ 独立 3V3 LDO + RST/EN |
| DO1 | `D6` 低边 MOSFET，告警联动演示（非 AI 直控） |
| 板载 FDCAN / LCD / Audio | 主板既有，扩展板不重复实现 |

网络优先级：RJ45 主链路 → ESP-01 备用 → 双不可用则离线，本地采集/告警继续。

**完整扇出、焊桥出厂/目标、BOM、布局与验收：**  
→ [`docs/velaguard-expansion-board.md`](docs/velaguard-expansion-board.md)

## 四、目录结构

```text
contest2026_004_TeamFalcons/
├── app/hello_app/                 # 官方 native app 模板（参考）
├── app/velaguard_app/             # VelaGuard 应用（Issue #01 骨架起）
├── board/contest_board/           # 板级配置模板
├── quickapp/hello_quickapp/       # 快应用模板（后续可选）
├── harness/                       # 本地流程 / Issue 检查脚本
├── scripts/                       # 补丁、Windows 构建与烧录辅助
├── logs/Foleaf/                   # AI Coding 日志（GitHub login）
├── docs/
│   ├── adr/                       # 架构决策
│   ├── agents/BOUNDARY.md         # 竞赛边界本地摘要（官方源优先）
│   ├── velaguard-expansion-board.md  # 扩展板硬件合同
│   └── windows_build_debug_setup.md  # Windows/WSL 调试说明
├── .scratch/.../issues/           # Issue 01–16 backlog
├── VelaGuard_项目手册.md
├── VelaGuard_推进方案.md
├── CONTEXT.md
├── CLAUDE.md
├── contest2026_004_TeamFalcons.xml
└── README.md
```

所有作品代码只放在本仓库内。不要直接修改父级 openvela 的 `nuttx/`、`apps/`、`packages/`、`vendor/`；公共仓变更须 fork 后向 `dev-ai-contest-2026` 提 PR。

官方 AI Skills 通过 `.claude` → `../.claude` 软链接使用。

## 五、运行方式

参赛仓通过 `contest2026_004_TeamFalcons.xml` 映射到 openvela 目录树；VelaGuard app 映射到 `packages/demos/contest2026_004_velaguard_app`。

### 1. 拉取工程

```bash
repo init -u https://github.com/open-vela/contest2026_004_TeamFalcons \
  -b dev-ai-contest-2026 -m contest2026_004_TeamFalcons.xml
repo sync -c -j8
```

### 2. 竞赛边界与日志

Agent 开发前阅读 [`docs/agents/BOUNDARY.md`](docs/agents/BOUNDARY.md)（官方远程规则始终优先）。

```bash
python3 ../.claude/skills/contest-log-collector/tools/validate-log.py logs/
```

日志采集器（开发机一次）：

```bash
bash ../.claude/skills/contest-log-collector/onboarding/install.sh \
  --team-id contest2026_004_TeamFalcons \
  --github-login Foleaf
bash ../.claude/skills/contest-log-collector/onboarding/verify-setup.sh
```

### 3. 本地检查与构建

```bash
# 参赛仓内（若脚本存在）
python3 harness/velaguard_issue1_check.py

# 链接 / 补丁（按当前 scripts 说明执行）
bash scripts/ensure-openvela-links.sh
# 例如 QSPI 等：bash scripts/apply-openvela-qspi-patch.sh

# 在 openvela 工作区根目录构建
cd /path/to/openvela
./build.sh stm32h750b-dk:lvgl -j8
```

Windows 侧构建/烧录/调试见 [`docs/windows_build_debug_setup.md`](docs/windows_build_debug_setup.md)。

具体 board config、烧录与验收以当前 issue 与项目手册为准。业务能力按 issue 递增，**完整工业闭环尚未全部合入**。

## 六、AI Coding 使用说明

本项目持续使用官方支持的 AI 工具（Claude Code / OpenCode / Codex 等）辅助需求拆解、方案、编码与调试，并按大赛要求归集对话日志。

- 日志目录：`logs/Foleaf/<YYYY-MM-DD>/<tool>__<session_id>.jsonl`
- 工作区内会话结束会自动写入 `logs/`；**不会自动 git push**
- 官方日志格式检查：`python3 ../.claude/skills/contest-log-collector/tools/validate-log.py logs/`
- 竞赛规则与边界记忆：[`docs/agents/BOUNDARY.md`](docs/agents/BOUNDARY.md)

完整对话见 `logs/`。请勿改写 JSONL 正文；若含隐私，在 commit 前删除对应会话文件。

## 七、文档索引

| 文档 | 说明 |
|------|------|
| [`VelaGuard_项目手册.md`](VelaGuard_项目手册.md) | 产品目标、架构、模块、验收 |
| [`VelaGuard_推进方案.md`](VelaGuard_推进方案.md) | 分阶段推进与里程碑 |
| [`docs/velaguard-expansion-board.md`](docs/velaguard-expansion-board.md) | 扩展板引脚/焊桥/BOM/验收（硬件合同） |
| [`docs/adr/`](docs/adr/) | 架构决策（独立网关、MQTT AI Bridge、OTA 等） |
| [`docs/agents/BOUNDARY.md`](docs/agents/BOUNDARY.md) | 竞赛边界本地摘要 |
| [`docs/windows_build_debug_setup.md`](docs/windows_build_debug_setup.md) | Windows/WSL 构建调试 |
| [`.scratch/.../issues/`](.scratch/velaguard-independent-edge-ai-gateway/issues/) | Issue 01–16 backlog |

## 八、当前进度与下一步

| 项 | 状态 |
|----|------|
| 官方工作区重建 / 产品文档 / issues / logs | 完成 |
| `app/velaguard_app` Issue #01 骨架 | 仓库内已有，按 issue 验收推进 |
| H750 扩展板方案（Arduino + STMod） | **已确认** → [`docs/velaguard-expansion-board.md`](docs/velaguard-expansion-board.md) |
| Issues 02+（Modbus / 安全环 / 网络 / MQTT / …） | backlog，按序实现 |
| 自定义 Skill / 主动执行场景证据 | 后续 issue 交付 |

下一步：在 Issue #01 可演示基线上推进 **Issue #02 nanoMODBUS RS485**（对齐扩展板 UART7 合同），再进入本地安全环与双模网络。
