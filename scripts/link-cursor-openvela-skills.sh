#!/usr/bin/env bash
# Link openvela official skills (.claude/skills) into Cursor project skills (.cursor/skills).
# Requires contest repo inside openvela workspace (sibling of .claude/).
set -eu

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SKILLS_SRC="$(cd "$REPO_ROOT/../.claude/skills" && pwd)"
DEST="$REPO_ROOT/.cursor/skills"

if [ ! -d "$SKILLS_SRC" ]; then
  echo "ERROR: $SKILLS_SRC not found. Run repo sync from openvela workspace root." >&2
  exit 1
fi

mkdir -p "$DEST"
for name in "$SKILLS_SRC"/*; do
  base=$(basename "$name")
  ln -sfn "../../.claude/skills/$base" "$DEST/$base"
done

echo "Linked $(ls -1 "$DEST" | wc -l) skills into $DEST"
