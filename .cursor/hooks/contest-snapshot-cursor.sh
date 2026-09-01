#!/usr/bin/env bash
# Project-level Cursor hook: delegate to machine-wide contest collector.
# Requires: bash ../.claude/skills/contest-log-collector/onboarding/install.sh (once per machine)
set -eu

GLOBAL_HOOK="${HOME}/.claude/contest-shared/contest-snapshot-cursor.sh"
if [ ! -x "$GLOBAL_HOOK" ]; then
  echo "[session-log] hook skipped: $GLOBAL_HOOK not found (run install.sh first)" >&2
  exit 0
fi

exec bash "$GLOBAL_HOOK"
