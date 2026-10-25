#!/bin/bash
# Эталонные вырезки для сценариев этапов RoadToProLevel — со снимков самого
# переноса: у оригинала этих возможностей не было, и сверять не с чем.
#
#   tools/port-golden.sh СЦЕНАРИЙ   например pro1
#
# Сценарий tests/screens/СЦЕНАРИЙ.txt проходит без экрана (--script), и
# области из tests/screens/exact.txt вырезаются в
# tests/screens/golden/СЦЕНАРИЙ/. Снимать заново — только когда поведение
# изменено нарочно: тест затем сторожит, чтобы оно не менялось ненароком.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
[ $# -eq 1 ] || { sed -n '2,12p' "$0"; exit 2; }
SC="$1"
BIN="$ROOT/build/debug/tjtm"
[ -x "$BIN" ] || { echo "port-golden.sh: сначала cmake --build --preset debug" >&2; exit 1; }
WORK="$ROOT/build/port-golden/$SC"
rm -rf "$WORK"
mkdir -p "$WORK/base" "$WORK/shots"
cp "$ROOT/Config.txt" "$WORK/base"/
cp -r "$ROOT/Resources" "$ROOT/Картинки" "$WORK/base"/
# Свои файлы сценария (набор тайлов, карта), как в тесте.
if [ -d "$ROOT/tests/screens/$SC.files" ]; then
    cp -r "$ROOT/tests/screens/$SC.files/." "$WORK/base"/
fi
# Ответы диалогам файлов, как в тесте: @BASE@ — каталог прогона.
DIALOGS=""
if [ -f "$ROOT/tests/screens/$SC.dialogs" ]; then
    DIALOGS="$WORK/dialogs.txt"
    sed "s|@BASE@|$WORK/base|g" "$ROOT/tests/screens/$SC.dialogs" > "$DIALOGS"
fi
SDL_VIDEODRIVER=offscreen TJTM_TEST_DIALOGS="$DIALOGS" "$BIN" --base "$WORK/base" \
    --script "$ROOT/tests/screens/$SC.txt" --shots "$WORK/shots"

OUT="$ROOT/tests/screens/golden/$SC"
rm -rf "$OUT"
mkdir -p "$OUT"
n=0
while read -r sc shot x y w h _; do
    [ "$sc" = "$SC" ] || continue
    convert "$WORK/shots/$shot.png" -crop "${w}x${h}+$x+$y" +repage -strip "PNG32:$OUT/${shot}_${x}_${y}.png"
    n=$((n + 1))
done < <(sed 's/#.*//' "$ROOT/tests/screens/exact.txt")
echo "вырезок: $n → $OUT"
