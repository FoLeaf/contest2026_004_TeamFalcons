# UI Safety and Remote Candidate Review

Status: incomplete

## Parent

.scratch/velaguard-independent-edge-ai-gateway/PRD.md

## What to build

Harden the operator interaction model around local and remote changes. VelaGuard should keep the home screen focused on status and alarms, route risky operations through settings/review screens, enforce risk-based confirmation, and prevent remote candidates from interrupting high-severity alarm handling.

## Acceptance criteria

- [ ] The home screen contains no dangerous or irreversible actions.
- [ ] Configuration actions are available through setup/settings/review screens.
- [ ] Low-risk actions use ordinary confirmation.
- [ ] Medium-risk actions require a second confirmation step.
- [ ] High-risk actions require long-press confirmation or a confirmation code.
- [ ] Remote Candidate Configurations enter a review list instead of becoming active.
- [ ] Remote Candidate Configurations do not take over the UI during high-severity Active Alarms.
- [ ] Review screens show source, requested change, risk level, and before/after summary.
- [ ] Reject, save as draft, test read, and confirm paths are available where appropriate.
- [ ] All UI confirmation and rejection actions write structured events with source.

## Blocked by

- .scratch/velaguard-independent-edge-ai-gateway/issues/03-local-safety-loop-with-modbus-sensor.md
- .scratch/velaguard-independent-edge-ai-gateway/issues/05-durable-config-store-and-schema-recovery.md
- .scratch/velaguard-independent-edge-ai-gateway/issues/10-natural-language-candidate-configuration-flow.md
