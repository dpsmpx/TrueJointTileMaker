#!/bin/bash
# Один сценарий — на оригинале и на переносе, снимки рядом.
#
#   tools/screens.sh СЦЕНАРИЙ [КАТАЛОГ]
#
# СЦЕНАРИЙ — текстовый файл (примеры в tests/screens/): по команде gui.sh
# в строке, без слова TARGET. Пустые строки и строки с # пропускаются.
#
#   click 390 440
#   key Escape g
#   sleep 0.5
#   shot 10_settings
#
# Снимки ложатся в КАТАЛОГ (по умолчанию build/screens/ИМЯ): ref/, port/,
# и для каждого снимка side/ИМЯ.png — эталон слева, перенос справа, —
# и diff/ИМЯ.png. В конце — число несовпавших пикселей по каждому снимку.
#
# Ноль ждать не стоит: эталон под Mono рисует текст и сглаженные края
# средствами libgdiplus, и они расходятся с переносом на точку
# (docs/porting.md, «Сверка экранов»). Холст и раскладка обязаны совпадать.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GUI="$ROOT/tools/gui.sh"

[ $# -ge 1 ] || { sed -n '2,20p' "$0"; exit 2; }
SCENARIO="$1"
[ -f "$SCENARIO" ] || { echo "screens.sh: нет $SCENARIO" >&2; exit 2; }
NAME="$(basename "$SCENARIO" .txt)"
OUT="${2:-$ROOT/build/screens/$NAME}"
# С оригиналом сверяется интерфейс оригинала: перенос — с его жестами и
# видом, без правил плана «Интерфейс» (docs/roadmap.md).
export TJTM_PORT_ARGS="--classic ${TJTM_PORT_ARGS:-}"
rm -rf "$OUT"
mkdir -p "$OUT/ref" "$OUT/port" "$OUT/side" "$OUT/diff"

run() {
    local target="$1" line cmd
    "$GUI" "$target" start >/dev/null
    while IFS= read -r line || [ -n "$line" ]; do
        line="${line%%#*}"
        read -r -a cmd <<< "$line"
        [ ${#cmd[@]} -gt 0 ] || continue
        case "${cmd[0]}" in
            sleep) sleep "${cmd[1]}" ;;
            shot) "$GUI" "$target" shot "$OUT/$target/${cmd[1]}.png" ;;
            *) "$GUI" "$target" "${cmd[@]}" ;;
        esac
    done < "$SCENARIO"
    "$GUI" "$target" stop
}

# Программа может закрыться посреди сценария (команда «Выход»): тогда
# следующие команды не найдут окна. Это ошибка сценария, а не повод
# оставлять окна висеть — поэтому stop при любом исходе.
for target in ref port; do
    if ! run "$target"; then
        echo "screens.sh: сценарий на $target прерван" >&2
        "$GUI" "$target" stop
    fi
done

status=0
for f in "$OUT"/ref/*.png; do
    [ -e "$f" ] || continue
    shot="$(basename "$f" .png)"
    p="$OUT/port/$shot.png"
    if [ ! -f "$p" ]; then
        echo "$shot: нет снимка переноса"
        status=1
        continue
    fi
    convert "$f" "$p" +append "$OUT/side/$shot.png"
    printf '%s: %s\n' "$shot" "$("$GUI" compare "$f" "$p" "$OUT/diff/$shot.png" || true)"
done
exit "$status"
