# 执行计划

1. 固定来源版本
   - 记录团队远程 README 分支 SHA。
   - 记录 `open-vela/docs` 的 `dev-ai-contest-2026` 分支 SHA。
   - 提取 README 全部唯一直接链接及导航页中与规则有关的二级链接。
2. 阅读官方材料
   - 通读大赛总览、代码提交指南、AI Coding 日志手册。
   - 通读三个赛道导航，重点阅读 AI 硬件赛道要求页。
   - 阅读远程 `logs/README.md` 和 CLA 页面可获取的正式说明。
   - 保存每条可核验规则的原文、文档路径和行号/标题。
3. 拆解 harness
   - 覆盖 `check_workspace`、`check_manifest`、`check_readme`、`check_template_residue`、`check_logs`、`validate_jsonl`、`check_ai_hardware`、`check_claude_md`、退出码与来源列表。
   - 逐分支建立规则来源矩阵，不把检查函数整体笼统判定。
4. 行为验证
   - 运行默认、strict、JSON 三种模式。
   - 针对来源矩阵中高风险断言构造不污染工作区的最小测试情形。
   - 检查错误严重级别、误报、漏报和可绕过性。
5. 形成报告
   - 先列有影响的问题，再列合理实现和官方要求遗漏。
   - 给出 harness 作为日常辅助与最终门禁的独立结论。
   - 提供可执行的修正优先级，但不修改 harness。
6. 复核
   - 确认每个 harness 检查类别都有审查结论。
   - 确认每个重要发现都有官方证据或明确标注“无官方依据”。
   - 检查报告未把团队约定写成赛事规则。
7. 执行用户批准的删除决定
   - 删除 `harness/contest_flow_check.py`、`harness/README.md`、`harness/REVIEW.md`。
   - 更新 `docs/agents/BOUNDARY.md`，移除本地门禁声明，加入官方日志验证和人工提交核对清单。
   - 更新 `README.md`、`CLAUDE.md` 中的目录树、命令和规则入口。
   - 搜索活动文件，确认没有遗留 harness 引用；历史 AI JSONL 和本任务审查报告不改写。

## 验证命令

```bash
python3 ../.claude/skills/contest-log-collector/tools/validate-log.py logs/
rg -n --hidden --glob '!logs/**' --glob '!.trellis/tasks/**' \
  'harness/|contest_flow_check|--strict' .
```

## 风险与回滚

- 风险：浮动分支内容变化。缓解：记录并引用提交 SHA。
- 风险：GitHub 页面渲染或登录限制。缓解：优先读取 raw 内容和 GitHub API；注明无法验证的页面。
- 风险：规则措辞存在“建议/应/必须”差异。缓解：保留原文并按权威等级分类，不自行升级。
- 回滚：本任务不修改 harness；任务文档可独立删除，不影响产品代码。
