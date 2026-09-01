#!/usr/bin/env bash
# Resolve a Windows-hosted tool path for WSL interop (OpenOCD, etc.).
#
# Usage:
#   scripts/wsl_resolve_windows_tool.sh <env-var-name> <win-candidate-1> ...
#
# Prints the first existing path (WSL /mnt/... form) and exits 0.
# Falls back to the first candidate when none exist (caller may still fail).

set -euo pipefail

if (($# < 2)); then
  echo "usage: $0 <env-var-name> <candidate>..." >&2
  exit 2
fi

env_name="$1"
shift

explicit="${!env_name:-}"
if [[ -n "${explicit}" ]]; then
  printf '%s\n' "${explicit}"
  exit 0
fi

for candidate in "$@"; do
  # Accept both D:\foo and /mnt/d/foo forms.
  wsl_path="${candidate}"
  if [[ "${candidate}" =~ ^[A-Za-z]: ]]; then
    drive_letter="$(echo "${candidate:0:1}" | tr 'A-Z' 'a-z')"
    rest="${candidate:2}"
    rest="${rest//\\//}"
    wsl_path="/mnt/${drive_letter}${rest}"
  fi
  if [[ -f "${wsl_path}" || -x "${wsl_path}" ]]; then
    printf '%s\n' "${wsl_path}"
    exit 0
  fi
done

printf '%s\n' "$1"
