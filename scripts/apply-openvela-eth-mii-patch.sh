#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
OPENVELA_ROOT="${1:-$(cd "$REPO_ROOT/.." && pwd)}"
NUTTX_ROOT="$OPENVELA_ROOT/nuttx"
PATCH_FILE="$SCRIPT_DIR/openvela-eth-mii-stm32h750b-dk.patch"

if [[ ! -d "$NUTTX_ROOT/.git" && ! -f "$NUTTX_ROOT/.git" ]]; then
  echo "error: NuttX git checkout not found: $NUTTX_ROOT" >&2
  exit 1
fi

if [[ ! -f "$PATCH_FILE" ]]; then
  echo "error: ETH MII patch not found: $PATCH_FILE" >&2
  exit 1
fi

if git -C "$NUTTX_ROOT" apply --reverse --check "$PATCH_FILE" >/dev/null 2>&1; then
  echo "ETH MII/QSPI pin-conflict patch is already applied."
  exit 0
fi

if ! git -C "$NUTTX_ROOT" apply --check "$PATCH_FILE"; then
  echo "error: ETH MII patch does not apply cleanly." >&2
  echo "Inspect overlapping changes in $NUTTX_ROOT before retrying." >&2
  exit 1
fi

git -C "$NUTTX_ROOT" apply "$PATCH_FILE"
echo "Applied STM32H750B-DK MII/QSPI pin-conflict patch."
