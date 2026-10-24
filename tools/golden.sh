#!/bin/bash
# Снимает эталонные данные с оригинала: tests/golden/*.txt.
#
#   tools/golden.sh            все эталоны
#   tools/golden.sh color map  только перечисленные (color config map editor png zlib)
#
# Харнессы из tools/golden/*.pas компилируются вместе с исходниками
# оригинала из pascal/ компилятором PascalABC.NET под Mono и пишут отчёты,
# которые тесты переноса воспроизводят на C++. Модулям, тянущим GraphABC,
# нужен экран — им поднимается Xvfb.
#
# Эталон снимается заново только при правке харнессов или входных файлов:
# оригинал в pascal/ не меняется.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TOOLS_DIR="${TJTM_TOOLS_DIR:-$HOME/.local/share/tjtm-tools}"
# shellcheck source=/dev/null
[ -f "$TOOLS_DIR/env.sh" ] && . "$TOOLS_DIR/env.sh"
PABCNET_DIR="${PABCNET_DIR:-$TOOLS_DIR/pascalabcnet}"
COMPILER="$PABCNET_DIR/bin/pabcnetc.exe"
[ -f "$COMPILER" ] || { echo "golden.sh: нет $COMPILER — сначала tools/setup-env.sh" >&2; exit 1; }

GOLDEN="$ROOT/tests/golden"
WORK="$ROOT/build/golden"
rm -rf "$WORK"
mkdir -p "$WORK"
cp "$ROOT"/pascal/*.pas "$ROOT"/tools/golden/*.pas "$WORK"/

compile() {
    { (cd "$WORK" && mono "$COMPILER" "$1.pas") | grep -v '^\[' >&2; } || true
    [ -f "$WORK/$1.exe" ] || { echo "golden.sh: $1 не собрался" >&2; exit 1; }
}

run_color() {
    compile GoldenColor
    xvfb-run -a mono "$WORK/GoldenColor.exe" "$GOLDEN/color.txt" 2>/dev/null
}

run_config() {
    compile GoldenConfig
    mono "$WORK/GoldenConfig.exe" "$GOLDEN/config_in" "$WORK"
    cp "$WORK/config.txt" "$WORK/config_saved_default.txt" "$WORK/config_saved_mixed.txt" "$GOLDEN"/
}

run_map() {
    compile GoldenMap
    mono "$WORK/GoldenMap.exe" "$GOLDEN/map_in" "$WORK"
    cp "$WORK/map.txt" "$GOLDEN"/
}

run_editor() {
    # Внутренности TJTM.pas: главный блок программы отрезается, вместо него
    # подставляется харнесс. Так проверяются процедуры, которые в модули
    # не вынесены, — кисть, фигуры, заливка, палитра, обзор.
    local src="$WORK/TJTM.pas" body="$WORK/GoldenEditorBody.pas"
    python3 - "$src" "$body" "$WORK/GoldenEditor.pas" <<'PY'
import sys
src, body, out = sys.argv[1:4]
text = open(src, encoding='utf-8-sig').read()
main = text.rstrip().rfind('\nbegin')
assert main > 0 and text.rstrip().endswith('end.'), 'не найден главный блок TJTM.pas'
open(out, 'w', encoding='utf-8-sig').write(text[:main] + '\n' + open(body, encoding='utf-8').read())
PY
    compile GoldenEditor
    (cd "$WORK" && xvfb-run -a mono GoldenEditor.exe "$GOLDEN" 2>/dev/null)
}

run_png() {
    # Эталон чтения PNG — сама libpng: PngSuite и картинки репозитория.
    cc -O2 -o "$WORK/pngref" "$ROOT/tools/golden/pngref.c" -lpng
    (cd "$ROOT" && "$WORK/pngref" tests/data/pngsuite/*.png Resources/*.png Resources/Icons/*.png \
        Картинки/*.png) > "$GOLDEN/png.txt"
}

run_zlib() {
    # Потоки настоящего zlib всех видов: без сжатия, фиксированные и
    # собственные коды, разные уровни. Распаковщик переноса обязан их понять.
    python3 - "$GOLDEN/zlib.txt" <<'PY'
import sys, zlib, random
random.seed(7)
inputs = {
    'empty': b'',
    'text': ('Любовь Эндора. ' * 60).encode('utf-8'),
    'random': bytes(random.getrandbits(8) for _ in range(700)),
    'runs': b''.join(bytes([i % 7]) * (i % 300 + 1) for i in range(60)),
    'pixels': bytes((x * 3 + y * 5) % 256 for y in range(24) for x in range(64)),
}
lines = []
for name, data in inputs.items():
    for level in (0, 1, 6, 9):
        for strategy, sname in ((zlib.Z_DEFAULT_STRATEGY, 'default'), (zlib.Z_FIXED, 'fixed'),
                                (zlib.Z_HUFFMAN_ONLY, 'huffman'), (zlib.Z_RLE, 'rle')):
            c = zlib.compressobj(level, zlib.DEFLATED, 15, 9, strategy)
            z = c.compress(data) + c.flush()
            lines.append(f'zlib {name}-{level}-{sname} {data.hex() or "-"} {z.hex()}')
open(sys.argv[1], 'w').write('\n'.join(lines) + '\n')
PY
}

targets=("$@")
[ "${#targets[@]}" -gt 0 ] || targets=(color config map editor png zlib)
for t in "${targets[@]}"; do
    echo "== $t" >&2
    "run_$t"
done
echo "эталоны записаны в tests/golden" >&2
