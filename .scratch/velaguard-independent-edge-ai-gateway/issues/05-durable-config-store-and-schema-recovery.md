# Durable Config Store and Schema Recovery

Status: incomplete

## Parent

.scratch/velaguard-independent-edge-ai-gateway/PRD.md

## What to build

Build the durable configuration layer used by sensor settings, network settings, alarm rules, and future Candidate Configurations. It should use dual-slot commit, schema versioning, checksums, migration rules, and safe fallback behavior so VelaGuard can still boot and run the Local Safety Loop after corruption or interrupted writes.

## Acceptance criteria

- [ ] Configuration is stored with two committed slots and metadata including `schema_version`, `seq`, checksum, and committed state.
- [ ] Writes are atomic from the caller's perspective and never replace the last valid committed configuration with an invalid one.
- [ ] Startup chooses the newest valid committed slot.
- [ ] If one slot is corrupt, startup loads the other slot and writes a warn/error event.
- [ ] If both slots are corrupt, startup enters factory/default configuration and still boots the app.
- [ ] Old supported schema versions migrate to the current schema.
- [ ] Future schema versions are rejected safely.
- [ ] Missing non-critical fields are defaulted with warn logs.
- [ ] Invalid critical fields disable only the affected module where possible.
- [ ] Failed sensor test reads may save a disabled draft but cannot become active Sensor Configurations.

## Blocked by

- .scratch/velaguard-independent-edge-ai-gateway/issues/01-bootable-velaguard-skeleton.md
