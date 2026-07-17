# Implement checklist: clean rebuild

## Done

- [x] Stage keep + MANIFEST (132 files)
- [x] Delete/rename old openvela
- [x] repo init + repo sync (retry 2 failed external projects)
- [x] Overlay keep assets
- [x] Reset issues 01–16 incomplete
- [x] Remove logs/your-github-login example
- [x] Branch feat/velaguard-restart
- [x] trellis init + create task 07-12-velaguard-full-clean-restart
- [x] Run harness/contest_flow_check.py (document residual fails)

## Not in this task

- [ ] Implement Issue #01
- [ ] Create app/velaguard_app + xml linkfile
- [ ] git commit (wait for user)

## Validation commands

```bash
python3 harness/contest_flow_check.py
test ! -d app/velaguard_app
test -d logs/Foleaf
grep -R '\- \[x\]' .scratch/velaguard-independent-edge-ai-gateway/issues/ || true
git branch --show-current  # feat/velaguard-restart
```

## Stop condition

Workspace is clean scaffold + product assets; await user command for Issue #01 or commit.

## Official pre-dev (2026-07-12 follow-up)

- [x] Link official skills: `.claude` → `../.claude`
- [x] contest-log-collector install + verify-setup (Foleaf)
- [x] `.gitignore` from example (+ local junk); logs not ignored
- [x] Durable rules: `docs/agents/BOUNDARY.md` + `CLAUDE.md`
- [x] Product README (required sections) for harness
- [x] Re-run harness after pre-dev setup
