#!/bin/bash
# Управление окнами редактора для сравнения переноса с оригиналом.
#
#   tools/gui.sh ref  build          собрать оригинал pascal/TJTM.pas под Mono
#   tools/gui.sh port build          собрать перенос (cmake, пресет debug)
#   tools/gui.sh TARGET start        запустить; без экрана — в своём Xvfb
#   tools/gui.sh TARGET stop         закрыть
#   tools/gui.sh TARGET shot F.png   снимок окна 780x560
#   tools/gui.sh TARGET key KEY...   клавиши: Escape, g, ctrl+z, Tab
#   tools/gui.sh TARGET click X Y [BTN]          щелчок; BTN 1/2/3, ролик 4/5
#   tools/gui.sh TARGET drag X0 Y0 X1 Y1 [BTN]   протяжка с нажатой кнопкой
#   tools/gui.sh TARGET type TEXT    набрать текст
#   tools/gui.sh compare A.png B.png [DIFF.png]  число несовпавших пикселей
#
# TARGET — ref (оригинал) или port (перенос). Координаты — внутри окна,
# как в коде программы: холст 0..599, панель 600..779.
#
# Один и тот же сценарий прогоняется на обоих, снимки сравниваются. У каждого
# свой виртуальный экран: окна на общем легли бы друг на друга, и щелчок
# достался бы тому, что сверху.
#
# Оригинал ищет Config.txt, Resources/ и Картинки/ рядом с exe и пишет
# в Config.txt при работе. Поэтому для него всё это копируется в build/ref,
# а не связывается ссылками: эталон не должен трогать файлы репозитория.
# Каждая сборка возвращает его настройки к тем, что лежат в репозитории.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TOOLS_DIR="${TJTM_TOOLS_DIR:-$HOME/.local/share/tjtm-tools}"
# shellcheck source=/dev/null
[ -f "$TOOLS_DIR/env.sh" ] && . "$TOOLS_DIR/env.sh"
PABCNET_DIR="${PABCNET_DIR:-$TOOLS_DIR/pascalabcnet}"
REF_DIR="$ROOT/build/ref"
PORT_BIN="$ROOT/build/debug/tjtm"
TITLE="True Joint Tile Maker"

die() { echo "gui.sh: $*" >&2; exit 1; }

usage() { sed -n '2,17p' "$0"; exit 2; }

# --- цели -------------------------------------------------------------------

build_ref() {
    local compiler="$PABCNET_DIR/bin/pabcnetc.exe"
    [ -f "$compiler" ] || die "нет $compiler — сначала tools/setup-env.sh"
    rm -rf "$REF_DIR"
    mkdir -p "$REF_DIR"
    cp "$ROOT"/pascal/*.pas "$ROOT/Config.txt" "$REF_DIR"/
    cp -r "$ROOT/Resources" "$ROOT/Картинки" "$REF_DIR"/
    # Компилятор кладёт exe рядом с исходником; строки с номерами шагов
    # ([12]BeginParsingFile...) — шум, остальное показываем.
    { (cd "$REF_DIR" && mono "$compiler" TJTM.pas) | grep -v '^\[' >&2; } || true
    [ -f "$REF_DIR/TJTM.exe" ] || die "оригинал не собрался"
    echo "$REF_DIR/TJTM.exe"
}

build_port() {
    (cd "$ROOT" && cmake --preset debug >/dev/null && cmake --build --preset debug) >&2
    [ -x "$PORT_BIN" ] || die "перенос не собрался"
    echo "$PORT_BIN"
}

launch() {
    case "$TARGET" in
        ref)
            [ -f "$REF_DIR/TJTM.exe" ] || build_ref >/dev/null
            (cd "$REF_DIR" && exec mono TJTM.exe) ;;
        port)
            [ -x "$PORT_BIN" ] || build_port >/dev/null
            # Программный кадровый буфер: Xvfb всё равно рисует без GPU.
            SDL_FRAMEBUFFER_ACCELERATION=0 exec "$PORT_BIN" ;;
    esac
}

# --- экран и окно -----------------------------------------------------------

ensure_display() {
    # Есть свой экран — работаем на нём. Нет — поднимаем Xvfb для этой цели.
    if [ -n "${DISPLAY:-}" ] && [ -z "${TJTM_GUI_FORCE_XVFB:-}" ]; then
        echo "$DISPLAY" > "$STATE/display"
        return
    fi
    export DISPLAY="$DEFAULT_DISPLAY"
    if ! xdotool getmouselocation >/dev/null 2>&1; then
        Xvfb "$DISPLAY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 &
        echo $! > "$STATE/xvfb.pid"
        for _ in $(seq 50); do
            xdotool getmouselocation >/dev/null 2>&1 && break
            sleep 0.1
        done
    fi
    echo "$DISPLAY" > "$STATE/display"
}

use_display() {
    [ -f "$STATE/display" ] || die "$TARGET не запущен: tools/gui.sh $TARGET start"
    DISPLAY="$(cat "$STATE/display")"
    export DISPLAY
}

window_id() {
    local wid
    wid="$(xdotool search --name "^$TITLE" 2>/dev/null | head -1 || true)"
    [ -n "$wid" ] || die "окно $TARGET не найдено"
    echo "$wid"
}

# --- команды ----------------------------------------------------------------

cmd_start() {
    ensure_display
    launch > "$STATE/run.log" 2>&1 &
    echo $! > "$STATE/app.pid"
    for _ in $(seq 100); do
        xdotool search --name "^$TITLE" >/dev/null 2>&1 && break
        sleep 0.1
    done
    window_id >/dev/null
    # Окно появляется раньше, чем дорисован первый кадр.
    sleep 1.5
    echo "$TARGET запущен на $DISPLAY, окно $(window_id)"
}

cmd_stop() {
    if [ -f "$STATE/app.pid" ]; then
        kill "$(cat "$STATE/app.pid")" 2>/dev/null || true
        rm -f "$STATE/app.pid"
    fi
    if [ -f "$STATE/xvfb.pid" ]; then
        kill "$(cat "$STATE/xvfb.pid")" 2>/dev/null || true
        rm -f "$STATE/xvfb.pid"
    fi
    rm -f "$STATE/display"
}

cmd_shot() {
    [ $# -eq 1 ] || die "shot FILE.png"
    use_display
    import -window "$(window_id)" "$1"
}

cmd_key() {
    use_display
    local wid
    wid="$(window_id)"
    for k in "$@"; do
        xdotool key --window "$wid" "$k"
        sleep 0.15
    done
}

cmd_click() {
    [ $# -ge 2 ] || die "click X Y [BTN]"
    use_display
    xdotool mousemove --window "$(window_id)" "$1" "$2" sleep 0.05 click "${3:-1}"
    sleep 0.15
}

cmd_drag() {
    [ $# -ge 4 ] || die "drag X0 Y0 X1 Y1 [BTN]"
    use_display
    local wid btn steps
    wid="$(window_id)"
    btn="${5:-1}"
    steps=12
    xdotool mousemove --window "$wid" "$1" "$2" sleep 0.05 mousedown "$btn"
    # Промежуточные точки: программа опрашивает мышь по кадрам, и прыжок
    # сразу в конец выглядел бы для неё как один щелчок.
    for i in $(seq 1 $steps); do
        xdotool mousemove --window "$wid" \
            $(($1 + ($3 - $1) * i / steps)) $(($2 + ($4 - $2) * i / steps)) sleep 0.03
    done
    xdotool mouseup "$btn"
    sleep 0.15
}

cmd_type() {
    use_display
    xdotool type --window "$(window_id)" --delay 30 "$*"
}

cmd_compare() {
    [ $# -ge 2 ] || die "compare A.png B.png [DIFF.png]"
    # AE — сколько пикселей отличается. Для пиксель-арта это честнее любой
    # «похожести»: один не тот пиксель — уже ошибка.
    local diff="${3:-null:}"
    local n
    n="$(compare -metric AE "$1" "$2" "$diff" 2>&1 >/dev/null || true)"
    echo "$n"
    [ "$n" = "0" ]
}

# --- разбор -----------------------------------------------------------------

[ $# -ge 1 ] || usage
if [ "$1" = "compare" ]; then
    shift
    cmd_compare "$@"
    exit $?
fi

TARGET="$1"
shift || true
case "$TARGET" in
    ref) DEFAULT_DISPLAY="${TJTM_REF_DISPLAY:-:99}" ;;
    port) DEFAULT_DISPLAY="${TJTM_PORT_DISPLAY:-:98}" ;;
    *) usage ;;
esac
STATE="$ROOT/build/gui/$TARGET"
mkdir -p "$STATE"

case "${1:-}" in
    build) if [ "$TARGET" = ref ]; then build_ref; else build_port; fi ;;
    start) cmd_start ;;
    stop) cmd_stop ;;
    shot) shift; cmd_shot "$@" ;;
    key) shift; cmd_key "$@" ;;
    click) shift; cmd_click "$@" ;;
    drag) shift; cmd_drag "$@" ;;
    type) shift; cmd_type "$@" ;;
    *) usage ;;
esac
