# `contest_flow_check.py` 合理性审查

审查日期：2026-07-12（Asia/Shanghai）

## 结论

`harness/contest_flow_check.py` **可以保留为开发期提示工具，但目前不适合作为竞赛合规证明，也不适合作为 PR / 最终提交门禁**。

- 默认模式对目录、manifest、README、日志布局做了一些有用的快速检查。
- `--strict` 不合理：它把启发式提示、开发环境状态和团队自己的 `CLAUDE.md` 约定全部升级为失败。当前仓库严格模式退出 1，但没有任何官方规则级 `FAIL`。
- 日志检查产生过强的安全感：它能把不符合官方 Schema 的 JSONL 判为 `PASS`。当前真实日志经官方验证器检查是合规的，但这是官方验证器给出的结论，不是本 harness 能证明的。
- AI 硬件检查只做关键词搜索，既能被文档文字轻易“满足”，也会因否定句或非标准目录产生误报。

建议将工具定位改成“仓库卫生与证据准备提示器”，并取消 `--strict` 的最终门禁含义；真正的日志门禁直接调用官方 `validate-log.py`，赛事要求中无法静态证明的项目明确列为 `MANUAL/UNVERIFIED`。

## 用户最终处置决定

用户在审阅结论后选择不再维护该竞赛检查器。`contest_flow_check.py`、配套 README/旧审查文件及缓存已删除；未创建替代脚本。`docs/agents/BOUNDARY.md` 成为唯一的本地 Agent 竞赛边界正文，README 与 CLAUDE 仅作为入口；日志格式直接使用官方 `validate-log.py` 验证。同目录中与竞赛规则无关的 QSPI 检查工具予以保留。

## 权威来源与版本

根来源及直接链接均从用户指定的团队远程 README 获取，而不是从本地 `BOUNDARY.md` 反推：

- 团队 README：`FoLeaf/contest2026_004_TeamFalcons@ab33f56d2ef3c209c76b307c039cf1fc9b9ace62`
  - [固定版本 README](https://github.com/FoLeaf/contest2026_004_TeamFalcons/blob/ab33f56d2ef3c209c76b307c039cf1fc9b9ace62/README.md)
  - `open-vela/contest2026_004_TeamFalcons` 同名分支在审查时解析到相同提交，README 内容无差异；harness 使用该 URL 不构成内容错误。
- 官方文档：`open-vela/docs@388302cbb5f9b17a42008dc005c1f53fde4b65de`
  - [大赛总览](https://github.com/open-vela/docs/blob/388302cbb5f9b17a42008dc005c1f53fde4b65de/zh-cn/contest_2026/contest_overview.md)
  - [参赛代码提交指南](https://github.com/open-vela/docs/blob/388302cbb5f9b17a42008dc005c1f53fde4b65de/zh-cn/contest_2026/code_submission_guide.md)
  - [AI Coding 日志手册](https://github.com/open-vela/docs/blob/388302cbb5f9b17a42008dc005c1f53fde4b65de/zh-cn/contest_2026/ai_coding_log_guide.md)
  - [快应用导航](https://github.com/open-vela/docs/blob/388302cbb5f9b17a42008dc005c1f53fde4b65de/zh-cn/contest_2026/quickapp/quickapp_guide_index.md)
  - [AI 硬件导航](https://github.com/open-vela/docs/blob/388302cbb5f9b17a42008dc005c1f53fde4b65de/zh-cn/contest_2026/ai_hardware/ai_hardware_guide_index.md)
  - [新硬件适配导航](https://github.com/open-vela/docs/blob/388302cbb5f9b17a42008dc005c1f53fde4b65de/zh-cn/contest_2026/hardware_porting/hardware_porting_guide_index.md)
- 团队 README 的其他直接链接：
  - [固定版本 logs/README.md](https://github.com/FoLeaf/contest2026_004_TeamFalcons/blob/ab33f56d2ef3c209c76b307c039cf1fc9b9ace62/logs/README.md)
  - [CLA 页面](https://openvela.com/#/community/cla)（审查时 HTTP 200；页面为前端应用，规则依据采用团队 README 中的 CLA 原文）
- AI 硬件导航中与 harness 断言直接相关的二级资料：
  - [AI 硬件赛道详细指引](https://github.com/open-vela/docs/blob/388302cbb5f9b17a42008dc005c1f53fde4b65de/zh-cn/contest_2026/ai_hardware/ai_hardware_track_guide.md)
  - [ai_agent 上手指南](https://github.com/open-vela/docs/blob/388302cbb5f9b17a42008dc005c1f53fde4b65de/zh-cn/contest_2026/ai_hardware/ai_agent_quickstart.md)
  - [mini-memo 指引](https://github.com/open-vela/docs/blob/388302cbb5f9b17a42008dc005c1f53fde4b65de/zh-cn/contest_2026/ai_hardware/mini_memo_guide.md)
- 日志手册指定的官方验证实现：`open-vela/.claude@e3c1f4bcb8afe8de4b6e3666b0ff4ec6b33930df`
  - [validate-log.py](https://github.com/open-vela/.claude/blob/e3c1f4bcb8afe8de4b6e3666b0ff4ec6b33930df/skills/contest-log-collector/tools/validate-log.py)
  - [event.schema.json](https://github.com/open-vela/.claude/blob/e3c1f4bcb8afe8de4b6e3666b0ff4ec6b33930df/skills/contest-log-collector/schema/event.schema.json)
  - [manifest.schema.json](https://github.com/open-vela/.claude/blob/e3c1f4bcb8afe8de4b6e3666b0ff4ec6b33930df/skills/contest-log-collector/schema/manifest.schema.json)

三个导航页已完整阅读。普通安装教程、硬件清单、芯片移植教程等二级链接没有被当作竞赛规则无限递归；AI 硬件导航的三份资料因被 harness 直接引用或用于支撑其断言而继续审阅。

## 主要发现

### H1 — High：日志 `PASS` 不代表符合官方日志规范

位置：`harness/contest_flow_check.py:323-415`

Harness 只验证：

- `manifest.json` 能被 `json.loads()` 解析；
- JSONL 每个非空行能被 `json.loads()` 解析；
- 存在整数 `seq` 时，列表与排序结果相等。

它没有验证官方 Schema 必填字段、字段类型和枚举，也没有验证 manifest 与 JSONL 的双向一致性、`team_id`、`github_login`、`tool`、`session_id`、`event_count`、孤儿文件或重复序号。官方日志手册明确要求运行 `validate-log.py`（日志手册第 263–267 行），官方验证器也明确实现这些检查（`validate-log.py` 第 5–10、78–190、194–298 行）。

实测内存夹具：以下内容都被本 harness 判为 `PASS`：

```jsonl
{"anything":"goes"}
```

```jsonl
{"seq":0}
{"seq":0}
```

```jsonl
{"seq":0}
{"seq":2}
```

影响：畸形、身份不一致或被破坏的日志可能通过所谓“最终提交检查”。

建议：不要复制一份简化规则；直接调用官方 `validate-log.py logs/`，并传播其 0/1/2/3 退出码。若必须离线内嵌，至少固定官方 Schema 版本并完整执行 Schema 与 manifest 关联检查。

补充：官方手册第 285–287 行声称 `validate-log.py` 会检测“序号断档”，但审查时固定的官方实现只检测重复与非递增序号，没有检测 `0, 2` 这种间隙。这是官方文档与官方工具自身的差异。Harness 至少不应声称比官方工具覆盖更完整；若团队希望落实手册文字，可在调用官方验证器后追加连续性检查，并明确它是对官方工具缺口的补充。

现状说明：官方验证器对当前真实 `logs/` 返回 `ALL OK`，检查了 102 个文件、11189 个事件。因此本发现是 harness 的能力缺陷，不表示当前日志不合规。

### H2 — High：`--strict` 把非官方、启发式和本机状态全部升级为提交失败

位置：`harness/contest_flow_check.py:159-168, 297-320, 486-501, 517-578`；`harness/README.md:18-25`

严格模式只要出现任意 `WARN` 就返回 1，没有区分：

- 官方硬性规则；
- 官方建议；
- 本地开发环境状态；
- 团队自己的 Agent 流程；
- 模糊关键词启发式。

当前实测：默认模式 `120 pass, 6 warn, 0 fail`、退出 0；严格模式对完全相同结果退出 1。其中 `.claude`、`.codex`、`.agents` 均为未跟踪文件或目录，却造成三个严格模式失败。官方日志手册只说采集器相关文件不进入 demo 仓，且 `.claude` 工具仓应位于 demo 仓同级（第 347–390 行）；它没有禁止任意 `.agents` / `.codex` 目录，更没有规定未跟踪的本机文件属于提交内容。

影响：`--strict` 既会阻塞合规提交，也无法证明通过者合规。

建议：取消“所有 WARN 都失败”。至少引入 `OFFICIAL_MUST`、`RECOMMENDATION`、`LOCAL_HYGIENE`、`MANUAL` 四类；只有有精确来源的 `OFFICIAL_MUST` 才能进入提交门禁。仓库内容检查应优先基于 `git ls-files`，而不是仅凭本地路径存在。

### H3 — High：把 README 的“建议章节”错误升级为强制 `FAIL`

位置：`harness/contest_flow_check.py:265-276`

团队官方 README 第 110–139 行要求提交前把模板替换为作品说明，但对五个示例章节的措辞是“**建议至少包含以下内容**”。Harness 将五个中文标题逐字匹配，并称其为 `official required submission sections`；缺少任意标题即 `FAIL`。

影响：内容完整但采用英文标题、合并章节或不同结构的 README 会被错误阻塞。

建议：保留“README 不得仍是模板”为硬检查；章节检查改为建议或按语义证据检查，并允许同义标题。运行方式和作品说明可作为高价值提示，但不应宣称五个标题逐字强制。

### H4 — High：将三个模板映射前缀误当成官方穷举白名单

位置：`harness/contest_flow_check.py:27-31, 217-228`

Harness 只允许：

- `packages/demos/`
- `packages/apps/`
- `vendor/openvela/boards/`

团队 README 第 47–55 行展示的是三种模板形态，并说明新增作品按同样规则增加子目录和 `<linkfile>`。AI 硬件详细指引第 118–122 行进一步允许 LVGL 应用、Demo、ai_agent 框架能力和自定义 Skill 按子目录组织并映射到 openvela 对应位置，并未把上述三个前缀声明为完整白名单。

影响：合法的 AI Agent 框架扩展或其他 openvela 目标路径可能被直接判 `FAIL`。

建议：保留绝对路径、`..` 逃逸和源不存在检查；把目标前缀改为模板已知值提示，或从官方 manifest / 项目配置声明中获取允许目标。未知目标应要求人工审查，而不是自动断定违规。

### M1 — Medium：AI 硬件赛题要求真实，但检测方法不能证明要求已满足

位置：`harness/contest_flow_check.py:418-483`

官方详细指引确实要求：设备运行、至少一个交互渠道、至少一个自定义 Skill 及演示、至少一个“主动+执行”场景，以及完整场景说明（第 51–85 行）。Harness 的判断存在双向错误：

- Skill 只搜索四个硬编码目录下的任意 `*.md`，未验证 Skill 格式、运行时 `/data/agent/skills/` 映射、内容或演示；真实 Skill 放在 `app/<name>/...` 等路径可能漏报，普通 Markdown 也可能误报。
- ai_agent 只在有限后缀文件中搜索几个字符串；Kconfig、CMake、C++、生成配置或其他集成方式可能漏报，注释也可误报。
- 主动场景在全仓文档与源码中搜索“主动/阈值/告警/alert/rule”等词。当前仓即使没有实现，也因产品文档出现相关词而得到 `PASS`。
- “聊天机器人”不处理否定语义；夹具文字“本项目不是聊天机器人；支持设备告警”同时得到“主动场景 PASS”和“聊天机器人 WARN”。

真实仓库复现更直接：写入本审查报告之前，harness 为 `120 pass, 6 warn`；报告写入 `.trellis/tasks/.../review.md` 后变为 `120 pass, 7 warn`，新增项正是 `Repository mentions chatbot-only language`。产品代码和产品要求没有改变，只是审查报告讨论了“聊天机器人”。原因是 `collect_text()` 会扫描未跟踪的 `.trellis/` 文档。这证明当前关键词结果会被开发过程元数据污染。

建议：将文档、源码、构建配置、设备运行证据和演示证据拆开报告。静态扫描最多输出 `EVIDENCE_FOUND`，不能输出赛题要求已经满足。最终状态必须保留人工/设备验证项。

### M2 — Medium：模板 README 检测可误杀已经替换的 README

位置：`harness/contest_flow_check.py:249-263`

只要 README 包含标题 `# contest2026_004_TeamFalcons`，即使其余内容已经完整替换，也会被判仍是官方模板。官方要求是替换使用说明内容，不是禁止作品使用仓库名作为标题。

建议：使用多个模板特征联合判断，或比较模板相似度；单独的仓库标题不能触发 `FAIL`。

### M3 — Medium：缺少“无法自动证明”的官方要求清单，造成覆盖完整的错觉

Harness 自称检查官方流程对齐，却没有报告以下官方事项为 `MANUAL/UNVERIFIED`：

- Apache 2.0 与原创性（大赛总览第 61–66、111–121 行）；
- 作品介绍文档、五分钟以内演示视频、专属仓地址（第 87–95 行）；
- 基于 openvela 的能力落地要求：图形 / AI / 多媒体至少一项（第 111–121 行）；
- 条件适用的统一语音唤醒词、MiMo Token 用途、服务端稳定性（同上）；
- AI 硬件的真机/指定硬件运行、交互渠道、Skill 演示和完整场景说明（AI 硬件指引第 51–85 行）；
- 公共仓修改必须走独立 fork + PR；代码中虽定义 `PUBLIC_TREE_PREFIXES`，但该常量从未使用。

这些事项有些不适合自动判定，问题不在于未强行自动化，而在于工具仍以“submission alignment / strict”呈现，却没有告诉用户哪些官方规则根本没被检查。

建议：报告尾部固定输出官方要求覆盖表，并把不可自动验证项标成 `MANUAL`，不得计入 PASS。

### M4 — Medium：本地目录名和 `.repo` 是开发环境线索，不是提交物合规性

位置：`harness/contest_flow_check.py:133-157`

官方确实规定专属远程仓命名，并推荐通过 `repo init/sync` 构建工作区；日志采集也依赖 `.repo`。但本地 clone 可以被用户改名，单独 clone 的提交物仍可能有效。严格模式把这两项升级为失败会混淆远程提交状态与本机开发状态。

建议：目录名和 `.repo` 保持开发环境提示；若检查提交身份，应检查 Git remote / manifest 项目身份，并明确这仍不能证明最终 PR 已合入。

### L1 — Low：`CLAUDE.md` 检查没有官方竞赛依据

位置：`harness/contest_flow_check.py:486-501`

官方文档没有要求参赛仓必须包含 `CLAUDE.md`，更没有要求它引用这个团队自制 harness。该检查属于团队 AI 工作流自检，不应出现在竞赛合规结果，更不应在 strict 模式造成失败。

建议：移入独立的团队开发检查器，或明确标记 `LOCAL_HYGIENE`。

### L2 — Low：来源和实现存在可维护性问题

位置：`harness/contest_flow_check.py:22-41, 535-537`

- 官方链接使用浮动分支，输出不记录审查版本，也没有逐检查来源映射。
- `SUPPORTED_LOG_TOOLS` 与 `PUBLIC_TREE_PREFIXES` 未被实际使用。
- 日志工具名称在不同官方材料间存在版本差异：团队模板 `logs/README.md` 和官方 Schema允许 `kiro`；较新的日志手册列出 Claude Code（含 AIoT-IDE）、AIoT-IDE、OpenCode、Codex。Harness 应以版本化 Schema 校验文件格式，并把“可计分工具”政策作为独立、可更新的规则，避免常量漂移。
- 每个日志文件输出一个 PASS，当前 102 个文件产生大量噪声，掩盖真正重要的警告。

建议：记录来源仓库与 SHA；每条规则绑定来源；成功项聚合，失败项保留逐文件细节。

## 逐类别来源矩阵

| Harness 类别 | 官方依据 | 判定 | 建议 |
|---|---|---|---|
| 本地目录等于 team id | 官方规定远程仓命名 | 部分准确；本地目录可改名 | 仅提示，远程身份另查 |
| 父目录存在 `.repo` | README 构建流程、日志工作区闸门 | 开发环境检查准确，不是提交合规 | 仅提示 |
| 禁止 `.claude/.codex/.agents` | 日志手册只禁止采集器文件进入 demo 仓 | 过宽且忽略 tracked 状态 | 只查已跟踪的已知采集器文件 |
| manifest 存在、XML 可解析、包含项目 | 团队 README 和模板 manifest | 合理的模板/构建完整性检查 | 保留，并注明属于构建契约 |
| linkfile 不逃逸、src 存在 | manifest 的基本安全和可构建性 | 合理的工程检查 | 保留 |
| linkfile 只能落到三个前缀 | README 示例，不是穷举规则 | 无充分官方依据 | 未知路径改人工审查 |
| README 存在并替换模板 | 团队 README 第六节 | 官方要求，方向正确 | 改进模板识别 |
| 五个固定中文章节 | 官方写“建议至少包含” | 严重级别错误 | WARN/语义检查 |
| README 声明 AI 硬件方向、提及 logs | README 建议、团队当前产品方向 | 有用提示，不是通用硬规则 | 保持提示 |
| team000 残留 | 模板定制的工程卫生 | 有用启发式，无独立官方硬规则 | WARN，不进入 strict |
| logs 目录和示例目录清理 | 团队 README、logs/README | 基本合理 | 保留；是否使用 AI 应条件化 |
| 日志目录/文件名布局 | logs/README、官方 Schema | 大体准确 | 使用官方 Schema |
| manifest/JSONL 内容 | 官方日志手册、Schema、验证器 | 明显不足 | 直接调用官方验证器 |
| 自定义 Skill | AI 硬件指引基础要求 | 规则准确，检测不可靠 | 分层证据 + 人工演示确认 |
| ai_agent/VelaClaw 字符串 | AI 硬件指引 | 仅弱证据 | 不得作为合规 PASS |
| 主动场景关键词 | AI 硬件指引基础要求 | 可轻易误报 | 文档/实现/运行证据分离 |
| `CLAUDE.md` 引用 harness | 无官方依据 | 纯团队约定 | 移出竞赛检查 |
| 任意 WARN 在 strict 中失败 | 无官方依据 | 不合理 | 只门禁官方 MUST |

## 合理且值得保留的部分

- 工具只读、离线，不自动 push，符合安全预期。
- manifest XML 解析、`src/dest` 缺失、路径逃逸和源目录不存在检查有实际工程价值。
- 检查 README 是否仍为完整模板、`logs/your-github-login` 是否未清理、日志文件是否为 JSONL，方向正确。
- JSON 输出适合 CI 消费，但需要增加规则 ID、权威等级、来源 SHA 和 `MANUAL` 状态。
- 默认模式将未完成的 AI 硬件工作先显示为提示，比直接阻断开发更合理；问题主要出在证据判定和 strict 升级策略。

## 建议修正顺序

1. P0：删除或重定义 `--strict` 的“所有 WARN 失败”行为；停止把它称为最终提交门禁。
2. P0：日志检查改为调用官方 `validate-log.py`；保留当前轻量检查只能作为预检查。
3. P1：给每个规则增加 `authority`、`source`、`source_sha` 和 `manual` 字段。
4. P1：修正 README “建议章节”与 linkfile 前缀的错误硬编码。
5. P1：重做 AI 硬件证据模型，区分文档、源码、构建、设备运行和演示。
6. P2：把 `.agents/.codex/CLAUDE.md`、模板残留等团队卫生项移到独立检查组。
7. P2：增加官方要求覆盖表与人工核对清单，聚合成功日志，减少 PASS 噪声。

## 验证记录

```text
python3 harness/contest_flow_check.py
=> 报告写入前：120 pass, 6 warn, 0 fail; exit 0
=> 报告写入后：120 pass, 7 warn, 0 fail; exit 0
   新增 WARN 来自本审查报告中的“聊天机器人”文字，而非产品状态变化

python3 harness/contest_flow_check.py --strict
=> 报告写入前：120 pass, 6 warn, 0 fail; exit 1
=> 报告写入后预期仍因任意 WARN 返回 1

python3 harness/contest_flow_check.py --json
=> JSON 输出成功；exit 0

python3 ../.claude/skills/contest-log-collector/tools/validate-log.py logs/ --quiet
=> 102 files, 11189 events, ALL OK; exit 0
```

审查阶段未修改 `harness/contest_flow_check.py`、产品代码或真实日志；审查完成后按用户决定删除了该竞赛检查器，并收敛了相关文档入口。真实日志始终未被改写。
