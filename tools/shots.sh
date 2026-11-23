#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The pictures in the docs, from the firmware itself (host mode): tools/shots.sh  ->  docs/images/*.png
set -e
cd "$(dirname "$0")/.."
PY="${PYTHON:-python3}"; [ -x .venv/bin/python ] && PY="$PWD/.venv/bin/python"
PATH="$(dirname "$PY"):$PATH" sh tests/run_host.sh tests/scripts/shots.txt >build/host/shots.log 2>&1 || { tail -20 build/host/shots.log; exit 1; }
"$PY" - <<'PYEOF'
from PIL import Image
names = {800: "boot-logo", 2200: "boot-plate", 3400: "training-flight", 11000: "playing", 12400: "line-up", 14200: "panel",
         15400: "edit", 16700: "code", 19000: "type-a-code"}
for t, n in names.items():
    Image.open(f"build/host/dinghy-{t}.png").convert("RGB").save(f"docs/images/{n}.png", optimize=True)
tiles = ["boot-logo", "playing", "line-up"]
hero = Image.new("RGB", (3 * 480 + 4 * 24, 480 + 48), (16, 16, 18))
for i, n in enumerate(tiles):
    hero.paste(Image.open(f"docs/images/{n}.png"), (24 + i * 504, 24))
hero.save("docs/images/hero.png", optimize=True)
tiles = ["training-flight", "panel", "edit", "code", "type-a-code", "boot-plate"]
sheet = Image.new("RGB", (3 * 480 + 4 * 24, 2 * 480 + 3 * 24), (16, 16, 18))
for i, n in enumerate(tiles):
    sheet.paste(Image.open(f"docs/images/{n}.png"), (24 + (i % 3) * 504, 24 + (i // 3) * 504))
sheet.save("docs/images/screens.png", optimize=True)
print("docs/images:", ", ".join(sorted(names.values())), "+ hero, screens")
PYEOF
