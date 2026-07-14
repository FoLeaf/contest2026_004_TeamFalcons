# Logging Guidelines

> How logging is done in this project.

---

## Overview

<!--
Document your project's logging conventions here.

Questions to answer:
- What logging library do you use?
- What are the log levels and when to use each?
- What should be logged?
- What should NOT be logged (PII, secrets)?
-->

(To be filled by the team)

---

## Log Levels

<!-- When to use each level: debug, info, warn, error -->

(To be filled by the team)

---

## Structured Logging

<!-- Log format, required fields -->

(To be filled by the team)

---

## What to Log

<!-- Important events to log -->

(To be filled by the team)

---

## What NOT to Log

<!-- Sensitive data, PII, secrets -->

(To be filled by the team)

---

## Scenario: Contest AI log secret redaction

### 1. Scope / Trigger

Apply this contract whenever Claude Code, Codex, or OpenCode conversation data
is exported under `logs/<github_login>/`. Tool calls and configuration excerpts
can contain credentials even when no application code logs them explicitly.

### 2. Signatures

```text
logs/Foleaf/redact.json
~/.claude/contest-collector-staging/Foleaf/redact.json
python3 /home/debian19y/openvela/.claude/skills/contest-log-collector/tools/validate-log.py --quiet logs
```

### 3. Contracts

- The repository `redact.json` is consumed by the OpenCode collector.
- The user staging `redact.json` is consumed by Claude Code/Codex collection.
- Custom rules must be generic patterns and must never contain an actual
  credential value.
- At minimum, redact quoted `apiKey` / `api_key` / `api-key` assignments with a
  value length of 16 or more, in addition to the collector defaults for
  `sk-*`, `ghp_*`, and `Bearer` headers.
- Historical redaction preserves `session_id`, `seq`, timestamps, role, and
  event count. Increment event `redacted_count` and manifest
  `redacted_count_total` for each replacement.
- One manifest entry points to one complete JSONL file. If a session was split
  across dates, merge by `seq` only after proving the sequence is unique,
  contiguous, and complete.

### 4. Validation & Error Matrix

| Condition | Required result |
| --- | --- |
| Exact credential bytes appear in any tracked/unignored repository file | Fail the security gate; report paths only, never matching content |
| Redaction rule contains a literal credential | Reject the rule |
| Replacement changes `session_id`, `seq`, or event count | Reject the rewrite and keep original files |
| Cross-date fragments have gaps or duplicate sequence numbers | Do not merge; report the affected session |
| Manifest count differs from its JSONL line count | Fail the official validator |
| Exact-byte scan is clean and official validator passes | Security/log gate passes |

### 5. Good / Base / Bad Cases

- **Good**: an exported config excerpt contains an `apiKey` assignment; the
  value becomes `***REDACTED***`, metadata counts increase, and validation
  passes.
- **Base**: a normal conversation contains no credential; the event is written
  unchanged.
- **Bad**: print the credential to shell output in order to grep for it.
- **Bad**: delete a complete contest session instead of preserving and
  redacting its events.

### 6. Tests Required

1. Use a fake 16+ character value to prove both the JavaScript OpenCode rule
   and Python staging rule redact `apiKey` assignments.
2. Load the real credential only in memory and scan repository bytes; output
   only PASS/FAIL or file paths.
3. Run the official validator over the complete `logs/` tree.
4. For fragment normalization, assert `seq == range(event_count)` before and
   after merging.

### 7. Wrong vs Correct

#### Wrong

```text
echo "$SECRET"
grep -R "$SECRET" logs/
```

This exposes the value in terminal output, process arguments, or captured AI
logs.

#### Correct

```text
Read secret in process memory → compare file bytes → print only PASS/FAIL
```

Persist only generic redaction patterns and `***REDACTED***` placeholders.
