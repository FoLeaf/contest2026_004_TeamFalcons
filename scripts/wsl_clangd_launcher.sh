#!/usr/bin/env bash
# Launch clangd for Cursor Remote - WSL IntelliSense.
#
# Resolution order:
#   1. CLANGD_BIN (explicit path)
#   2. newest ~/tools/clangd/clangd_*/bin/clangd
#   3. clangd on PATH (apt / extension-managed)

set -euo pipefail

if [[ -n "${CLANGD_BIN:-}" && -x "${CLANGD_BIN}" ]]; then
  exec "${CLANGD_BIN}" "$@"
fi

tools_root="${HOME}/tools/clangd"
if [[ -d "${tools_root}" ]]; then
  mapfile -t candidates < <(find "${tools_root}" -type f -name clangd -perm -111 2>/dev/null | sort -V)
  if ((${#candidates[@]} > 0)); then
    exec "${candidates[-1]}" "$@"
  fi
fi

if command -v clangd >/dev/null 2>&1; then
  exec clangd "$@"
fi

cat >&2 <<EOF
error: clangd not found.

Install one of:
  1. Cursor extension "clangd" (llvm-vs-code-extensions.vscode-clangd) in Remote - WSL
  2. apt install clangd
  3. Extract LLVM clangd under ~/tools/clangd/clangd_<version>/bin/clangd

Or set CLANGD_BIN to the clangd executable.
EOF
exit 1
