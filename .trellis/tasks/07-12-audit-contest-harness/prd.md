# 审查竞赛规则检查器合理性

## Goal

以团队竞赛仓库 `dev-ai-contest-2026` 分支的远程 README 及其直接链接的官方规则文档为权威依据，审查本地 `harness/contest_flow_check.py` 是否忠实、合理地实现了可自动检查的竞赛约束。

## Background

- `harness/` 是团队自行维护的检查器，不是赛事官方提供的文件。
- 本地 `docs/agents/BOUNDARY.md`、`CLAUDE.md`、Trellis 文档只能作为待核对材料，不能反向证明 harness 正确。
- 审查需要区分赛事硬性规则、官方建议、团队约定和产品自身约束，避免把非官方要求误报为竞赛失败。

## Requirements

- 获取并保存审查时使用的远程 README 内容、最终 URL/提交身份和所有与竞赛规则有关的直接超链接清单。
- 阅读 README 直接链接的官方竞赛文档；对重复导航、非规则性网页和进一步递归链接明确边界。
- 对 `harness/contest_flow_check.py` 的每个检查项追溯规则来源，并判断为：准确、部分准确、误判风险、无官方依据或遗漏。
- 审查默认模式与 `--strict` 的严重级别是否符合官方措辞，尤其检查是否把建议或团队偏好升级为硬性失败。
- 检查 harness 的实现质量，包括作用域、路径假设、占位符检测、日志格式检查、误报/漏报和可维护性。
- 形成有证据的审查报告，按严重程度列出问题，并提供文件行号、官方原文或精确来源链接。
- 本任务只做审查；除非用户另行授权，不修改 harness、规则记忆文档或产品代码。
- 审查完成后，用户明确授权删除本地竞赛规则 harness，不再保留或重写 `contest_flow_check.py`；同目录中与竞赛规则无关的 QSPI 工具不在本任务范围内。
- 以 `docs/agents/BOUNDARY.md` 作为唯一面向 Agent 的长期竞赛边界文档；其他入口只链接该文件，不复制第二套规则正文。
- 清除活动文档中对 `harness/contest_flow_check.py`、`--strict` 和 harness 目录的引用。
- 日志合规验证直接使用官方 `validate-log.py`，不再维护本地简化实现。

## Acceptance Criteria

- [x] 官方依据来自远程 README 及其链接，而不是本地摘要文件。
- [x] 提供已审阅文档清单，并注明抓取的分支/提交或其他可复现身份。
- [x] harness 的全部检查类别均完成来源映射，不只抽查少数规则。
- [x] 报告清楚区分官方强制项、官方建议、团队约定及纯实现质量问题。
- [x] 每个重要发现包含严重级别、证据、影响和建议修正方向。
- [x] 明确说明 harness 当前是否适合作为日常检查和提交门禁，以及结论的限制。
- [x] 不对工作区实施超出审查范围的修改。
- [x] `contest_flow_check.py`、配套文档和缓存已删除，且活动文档不存在对它的引用。
- [x] `docs/agents/BOUNDARY.md` 保留官方来源、硬边界、人工核对项和官方日志验证命令。
- [x] README 与 Agent 入口只指向 `docs/agents/BOUNDARY.md`，不再宣称存在本地竞赛门禁。

## Out of Scope

- 重写或替换 `harness/contest_flow_check.py`。
- 替赛事组织方解释未公开或表述含糊的政策。
- 审查 VelaGuard 产品功能是否满足最终评分要求。
