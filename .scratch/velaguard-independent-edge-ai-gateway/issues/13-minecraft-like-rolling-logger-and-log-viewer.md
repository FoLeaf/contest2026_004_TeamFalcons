# Minecraft-like Rolling Logger and Log Viewer

Status: incomplete

## Parent

.scratch/velaguard-independent-edge-ai-gateway/PRD.md

## What to build

Implement the Log4j2-inspired, Minecraft-like local logging model. VelaGuard should keep human-readable latest/debug/archive logs and structured events, expose severity/category filtering, redact secrets, and keep logging failures from interfering with acquisition and alarms.

## Acceptance criteria

- [ ] Logging supports levels `debug`, `info`, `warn`, and `error`.
- [ ] Log events include timestamp, logger/category, level, message, and key-value fields.
- [ ] `latest.log` contains current human-readable logs.
- [ ] Optional `debug.log` contains debug-level detail when enabled.
- [ ] `archive/*.log` stores rotated human-readable logs.
- [ ] `events.jsonl` stores structured business events.
- [ ] Different appenders can use different minimum levels and category filters for file, serial, UI, and cloud.
- [ ] UI log viewer filters by severity and category.
- [ ] Full secrets, tokens, API keys, and private signing material are redacted.
- [ ] Log truncation or a bad JSONL record does not block startup or the Local Safety Loop.
- [ ] Cloud upload defaults to structured events and key warn/error summaries, not full `latest.log`.

## Blocked by

- .scratch/velaguard-independent-edge-ai-gateway/issues/01-bootable-velaguard-skeleton.md
- .scratch/velaguard-independent-edge-ai-gateway/issues/08-event-ids-time-quality-and-pending-event-resend.md
