#!/usr/bin/env bash
# Regenerate gui/main/ui/fonts/vg_font_ui_14.c from UI string CJK + cjk_symbols.txt
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
python3 <<'PY'
import re, pathlib
ui = pathlib.Path('gui/main/ui')
sym_path = ui / 'fonts/cjk_symbols.txt'
str_re = re.compile(r'"([^"\\]*(?:\\.[^"\\]*)*)"')
chars = set()
for p in ui.rglob('*.c'):
    if 'fonts' in p.parts:
        continue
    for m in str_re.finditer(p.read_text(encoding='utf-8')):
        s = m.group(1).replace('\\n', '\n')
        for c in s:
            if ord(c) >= 0x80:
                chars.add(c)
chars.update(list('【】；·–…→、「」（）：？，。！℃'))
old = sym_path.read_text(encoding='utf-8').split()
seen, out = set(), []
for t in old:
    if t not in seen:
        seen.add(t); out.append(t)
for c in sorted(chars, key=ord):
    if c not in seen and not (32 <= ord(c) < 127):
        seen.add(c); out.append(c)
sym_path.write_text(' '.join(out) + '\n', encoding='utf-8')
print(f'cjk_symbols.txt tokens={len(out)}')
PY
SYMS=$(tr -d '\n' < gui/main/ui/fonts/cjk_symbols.txt)
FONT=${FONT:-/mnt/c/Windows/Fonts/simhei.ttf}
npx --yes lv_font_conv@1.5.2 \
  --font "$FONT" --size 14 --bpp 4 --format lvgl \
  -r 0x20-0x7E --symbols "$SYMS" --no-compress \
  -o gui/main/ui/fonts/vg_font_ui_14.c
echo "regenerated gui/main/ui/fonts/vg_font_ui_14.c"
