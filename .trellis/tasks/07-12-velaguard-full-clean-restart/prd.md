# PRD: VelaGuard full clean restart (repo rebuild only)

## Goal

Rebuild the openvela workspace from official upstream and restore product definition assets only, so Issue #01 can be implemented later on a clean tree.

## In scope

- Full wipe of previous local openvela tree (no full-tree backup; keep-pack only)
- `repo init` + `repo sync` from `dev-ai-contest-2026` / `contest2026_004_TeamFalcons.xml`
- Overlay keep assets:
  - `logs/Foleaf/`
  - `.scratch/velaguard-independent-edge-ai-gateway/` (PRD + issues 01–16)
  - `VelaGuard_项目手册.md`, `VelaGuard_推进方案.md`
  - `harness/`
  - `CONTEXT.md`, `docs/adr/`, `docs/agents/`
- Reset issues 01–16 to incomplete (all acceptance unchecked)
- Branch `feat/velaguard-restart` from official scaffold
- Trellis task + planning for this rebuild

## Out of scope

- Implementing Issue #01 skeleton or any `app/velaguard_app` code
- Restoring `scripts/`, parent-tree patches, debug docs, old restart history
- Adding `velaguard_app` linkfile to contest xml (deferred to #01 work)
- git commit/push unless user explicitly requests

## Acceptance criteria

- [x] Keep pack staged and MANIFEST validated before delete
- [x] openvela rebuilt via official repo init/sync (no missing projects after retries)
- [x] Keep assets overlaid; no old `app/velaguard_app` business code
- [x] Issues 01–16 status incomplete and acceptance unchecked
- [x] Branch `feat/velaguard-restart` exists from official base
- [x] Harness run recorded; known residual fails documented (README template / template residue)
- [ ] User-approved commit of overlay (optional, deferred)

## Constraints

- Contest code only under `contest2026_004_TeamFalcons/`
- No whole-tree backup by user request
- Do not rewrite AI log JSONL bodies
