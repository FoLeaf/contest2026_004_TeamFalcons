#!/usr/bin/env bash
# 从 secrets/ 加密写入板端 eMMC（COM3）。
#   bash scripts/provision-llm-from-secrets.sh [COM3]
set -euo pipefail
COM="${1:-COM3}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
powershell.exe -ExecutionPolicy Bypass -File "$SCRIPT_DIR/provision-llm-from-secrets.ps1" -ComPort "$COM"
