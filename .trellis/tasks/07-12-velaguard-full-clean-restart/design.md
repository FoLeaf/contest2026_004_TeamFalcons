# Design: full clean restart (rebuild only)

## Approach

1. Stage keep-pack to `~/velaguard-keep` with SHA256 MANIFEST (only recoverable copy)
2. Rename/delete old openvela; `repo init` + `repo sync -c -j8` (retry failed network projects with `-j1`)
3. Overlay keep into contest project; reset issue statuses
4. Create clean branch `feat/velaguard-restart` from `openvela/dev-ai-contest-2026`
5. Stop before any product implementation

## Git / branch

- Base: official `ab33f56` scaffold (`dev-ai-contest-2026`)
- Work branch: `feat/velaguard-restart` (new history; old local restart commits discarded)
- Remote `fork` = `FoLeaf/contest2026_004_TeamFalcons` for read-only reference (`feat/velaguard-poc`)

## Manifest / app shell

- Keep official xml without `velaguard_app` linkfile until #01 implementation
- No `app/velaguard_app` directory in this task

## Issue policy

- Preserve issue text/order
- Force `Status: incomplete` and `- [ ]` on all acceptance criteria
- Strip prior “accepted on restart” verification notes

## Risks

| Risk | Mitigation |
|------|------------|
| Network TLS failures mid-sync | Retry failed projects; confirm `repo status` missing=0 |
| Missing keep file | MANIFEST hard gate before delete |
| Harness FAIL on template README | Expected until project README rewrite; not blocking rebuild |
| Example logs dir | Remove `logs/your-github-login` after overlay |

## Rollback

- Without full backup: only re-overlay from `~/velaguard-keep` or re-fetch docs from `fork/feat/velaguard-poc`
- Logs only recoverable if keep-pack retained
