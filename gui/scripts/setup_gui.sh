#!/usr/bin/env bash
# Initialize gui/ LVGL tree for WSL/Linux PC simulator build.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

if [[ ! -d lvgl/.git ]]; then
  echo "[setup_gui] cloning lvgl v9.1.0..."
  git clone --depth 1 --branch v9.1.0 https://github.com/lvgl/lvgl.git lvgl
fi

if ! pkg-config --exists sdl2 2>/dev/null; then
  echo "[setup_gui] SDL2 dev package missing. Install:"
  echo "  sudo apt-get install -y libsdl2-dev cmake build-essential"
  exit 1
fi

echo "[setup_gui] OK — run:"
echo "  cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug"
echo "  cmake --build build -j\$(nproc)"
echo "  ./bin/main"
