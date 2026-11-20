#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Melisma's checks, on this computer (no hosted CI: see README.md).
#   tools/check.sh           the engine's tests, the golden pieces, a host run
#   tools/check.sh --full    then the device build, the platform's tests, and the image booted in fm1-emu
set -e
cd "$(dirname "$0")/.."
OUT=build/host
mkdir -p "$OUT"
cc -O2 -Wall -Wextra -Wno-unused-function -Wno-unused-const-variable -Ifirmware/src -o "$OUT/mel_test" tests/mel_test.c
"$OUT/mel_test" | tail -2
for g in tests/golden/*.txt; do
    "$OUT/mel_test" dump "$(basename "$g" .txt)" | cmp -s - "$g" || { echo "golden: $g differs (the music changed: mean it, then regenerate)"; exit 1; }
done
echo "golden   $(ls tests/golden | wc -l | tr -d ' ') pieces, note for note"
PY="${PYTHON:-python3}"; [ -x .venv/bin/python ] && PY="$PWD/.venv/bin/python"
PATH="$(dirname "$PY"):$PATH" sh tests/run_host.sh tests/scripts/melisma.txt >"$OUT/host.log" 2>&1 || { tail -20 "$OUT/host.log"; exit 1; }
echo "host     $(grep -c 'screen ->' "$OUT/host.log") screens, $(grep 'sound' "$OUT/host.log" | sed 's/.*(\(.*\))/\1/')"
[ "$1" = "--full" ] || exit 0

export JIELI_TOOLCHAIN="${JIELI_TOOLCHAIN:-$PWD/.jieli/toolchain}" AC79_SDK="${AC79_SDK:-$PWD/.sdk}" PYTHON="$PY"
PATH="$(dirname "$PY"):$PATH" ./build.sh | grep -E 'ok |package|error' 
PATH="$(dirname "$PY"):$PATH" sh tests/run_tests.sh >"$OUT/tests.log" 2>&1 || { grep -E 'FAIL|differs|error' "$OUT/tests.log" | head; exit 1; }
echo "platform $(tail -1 "$OUT/tests.log")"
EMU="$(ls -d .tools/fm1-emu/*/fm1-live 2>/dev/null | head -1)"
if [ -n "$EMU" ]; then                         # the real image, on the newer flash chip, at an FM-1's own pace
    mkdir -p build/emu && cd build/emu
    FM1_STRICT=1 FM1_BENCH_KEYS='2500:PLAY,2600:PLAY^,3000:e6:+21,4000:C4@100,4500:C4^,9000:PLAY,9100:PLAY^' FM1_BENCH_WAV=check.raw \
        "../../$EMU" --fwsc ../dinghy.fwsc --flash-chip uc25hq80 --bench 12000 >check.log 2>&1 || { tail -5 check.log; exit 1; }
    "$PY" - <<'PYEOF'
import array, sys
a = array.array('h'); a.frombytes(open('check.raw', 'rb').read()); L = a[0::2]
rep = sum(1 for b in range(1, len(L) // 256) if L[b * 256:(b + 1) * 256] == L[(b - 1) * 256:b * 256] and any(L[b * 256:(b + 1) * 256]))
peak = max(abs(x) for x in a) if a else 0
print(f"emulator peak {peak}, {rep} audio blocks repeated (a late render)")
sys.exit(0 if peak > 500 and rep == 0 else 1)
PYEOF
else
    echo "emulator skipped: no fm1-emu in .tools/ (github.com/joakimeriksson/fm1-emu releases)"
fi
