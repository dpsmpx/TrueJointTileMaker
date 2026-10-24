#!/bin/bash
# Установка всего, что нужно для переноса True Joint Tile Maker на C++.
#
#   tools/setup-env.sh [--skip-apt] [--skip-sdl] [--skip-pabcnet] [--skip-corpus]
#
# Ставит три вещи:
#
#   1. Системные пакеты: компиляторы и отладчики C++, GoogleTest,
#      анализаторы кода, Xvfb и xdotool для снимков экрана.
#   2. SDL3 с SDL3_ttf и SDL3_image, собранные из исходников: в Ubuntu 24.04
#      SDL3 ещё нет. Ставятся в $TJTM_TOOLS_DIR/sdl3, CMake находит их
#      через CMAKE_PREFIX_PATH из env.sh.
#   3. Эталон — компилятор PascalABC.NET, собранный из исходников под Mono.
#      Им собирается старая реализация из pascal/, чтобы сравнивать перенос
#      с оригиналом не по памяти, а по живой программе.
#   4. Корпус карт и тайлов игры Andors-Love: на нём проверяется, что разбор
#      и запись карт дают файл байт в байт.
#
# Рассчитан на Ubuntu 24.04. Повторный запуск ничего не пересобирает, если
# собранное уже на месте, поэтому годится и для SessionStart-хука.
#
# Куда кладётся: $TJTM_TOOLS_DIR, по умолчанию ~/.local/share/tjtm-tools.
# Там же лежит env.sh с путями, его подключают остальные скрипты.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TOOLS_DIR="${TJTM_TOOLS_DIR:-$HOME/.local/share/tjtm-tools}"

# Эталонный компилятор закреплён на проверенном коммите: заплатка для Mono
# написана под него, а более свежий может разойтись с ней.
PABCNET_URL="https://github.com/pascalabcnet/pascalabcnet"
PABCNET_REV="${PABCNET_REV:-643514c2d9b843ffb4971345b117dd2f6df6369c}"
PABCNET_DIR="$TOOLS_DIR/pascalabcnet"
PABCNET_PATCH="$ROOT/tools/pabcnet/mono68-generic-array-ctors.patch"

# Корпус игры, наоборот, берётся свежим: формат карт — договор с игрой,
# и сверяться надо с тем, что в ней сейчас.
ANDORS_URL="https://github.com/dpsmpx/Andors-Love"
ANDORS_DIR="$TOOLS_DIR/andors-love"

# SDL3 закреплена на выпусках: поведение окна, ввода и шрифтов между ними
# меняется, а снимки экрана сверяются попиксельно.
SDL_PREFIX="$TOOLS_DIR/sdl3"
SDL_SRC="$TOOLS_DIR/sdl3-src"
SDL_TAG=release-3.4.18
SDL_TTF_TAG=release-3.2.2
SDL_IMAGE_TAG=release-3.4.8

APT_PACKAGES=(
    # Сборка и отладка C++
    build-essential cmake ninja-build pkg-config ccache
    clang clang-tidy clang-format cppcheck shellcheck gdb lldb valgrind
    # Зависимости сборки SDL3: X11, Wayland, ввод, D-Bus для диалогов
    # через портал; FreeType и HarfBuzz для SDL3_ttf; PNG и JPEG для SDL3_image
    libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxss-dev
    libxfixes-dev libxtst-dev libxkbcommon-dev libwayland-dev wayland-protocols
    libegl-dev libgl-dev libdbus-1-dev libudev-dev
    libfreetype-dev libharfbuzz-dev libpng-dev libjpeg-dev zlib1g-dev
    # Диалоги открытия и сохранения SDL3 в Linux без портала идут через zenity
    zenity
    # Тесты
    libgtest-dev libgmock-dev
    # Шрифты: Liberation Sans метрически совпадает с Arial,
    # которым GraphABC пишет весь интерфейс
    fonts-liberation fonts-dejavu-core
    # Эталон: PascalABC.NET под Mono, WinForms поверх libgdiplus
    mono-complete libgdiplus dotnet-sdk-10.0
    # Запуск окон без экрана, управление ими и сравнение снимков
    xvfb xdotool imagemagick
    # Windows-сборка из Linux (пресет mingw) и её тесты под Wine
    mingw-w64 wine64
    git python3
)

SKIP_APT=0
SKIP_SDL=0
SKIP_PABCNET=0
SKIP_CORPUS=0
for arg in "$@"; do
    case "$arg" in
        --skip-apt) SKIP_APT=1 ;;
        --skip-sdl) SKIP_SDL=1 ;;
        --skip-pabcnet) SKIP_PABCNET=1 ;;
        --skip-corpus) SKIP_CORPUS=1 ;;
        -h|--help) sed -n '2,20p' "$0"; exit 0 ;;
        *) echo "неизвестный ключ: $arg" >&2; exit 2 ;;
    esac
done

log() { printf '[setup-env] %s\n' "$*" >&2; }

as_root() {
    if [ "$(id -u)" -eq 0 ]; then "$@"; else sudo "$@"; fi
}

install_packages() {
    local missing=()
    for p in "${APT_PACKAGES[@]}"; do
        dpkg-query -W -f='${Status}' "$p" 2>/dev/null | grep -q "install ok installed" || missing+=("$p")
    done
    if [ "${#missing[@]}" -eq 0 ]; then
        log "системные пакеты уже стоят"
        return
    fi
    log "ставлю пакеты: ${missing[*]}"
    as_root apt-get update -q
    as_root env DEBIAN_FRONTEND=noninteractive \
        apt-get install -y -q --no-install-recommends "${missing[@]}"
}

# Собирает одну библиотеку SDL3 из исходников и ставит её в SDL_PREFIX.
build_sdl_one() {
    local name="$1" tag="$2"
    shift 2
    local src="$SDL_SRC/$name" bld="$SDL_SRC/$name-build"
    if [ ! -d "$src/.git" ]; then
        git clone -q --depth 1 --branch "$tag" "https://github.com/libsdl-org/$name" "$src"
    else
        git -C "$src" fetch -q --depth 1 origin tag "$tag"
        git -C "$src" checkout -q --force "$tag"
    fi
    cmake -S "$src" -B "$bld" -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$SDL_PREFIX" -DCMAKE_PREFIX_PATH="$SDL_PREFIX" \
        -DCMAKE_INSTALL_LIBDIR=lib "$@" > /dev/null
    cmake --build "$bld" > /dev/null
    cmake --install "$bld" > /dev/null
}

build_sdl() {
    local stamp_file="$SDL_PREFIX/.tjtm-stamp"
    local want="$SDL_TAG $SDL_TTF_TAG $SDL_IMAGE_TAG"
    if [ "$(cat "$stamp_file" 2>/dev/null)" = "$want" ]; then
        log "SDL3 уже собрана: $SDL_PREFIX"
        return
    fi
    mkdir -p "$SDL_SRC"
    log "собираю SDL3 ($want)"
    build_sdl_one SDL "$SDL_TAG" -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF
    # FreeType и HarfBuzz берутся системные: так быстрее, а на Windows
    # CMakeLists переноса собирает их встроенными (SDLTTF_VENDORED).
    build_sdl_one SDL_ttf "$SDL_TTF_TAG" -DSDLTTF_SAMPLES=OFF -DSDLTTF_VENDORED=OFF
    build_sdl_one SDL_image "$SDL_IMAGE_TAG" -DSDLIMAGE_SAMPLES=OFF -DSDLIMAGE_TESTS=OFF \
        -DSDLIMAGE_VENDORED=OFF -DSDLIMAGE_AVIF=OFF -DSDLIMAGE_JXL=OFF \
        -DSDLIMAGE_TIF=OFF -DSDLIMAGE_WEBP=OFF
    echo "$want" > "$stamp_file"
    log "SDL3 готова"
}

# Отпечаток сборки: коммит и заплатка. Совпал — собирать незачем.
pabcnet_stamp() {
    printf '%s %s\n' "$PABCNET_REV" "$(sha256sum "$PABCNET_PATCH" | cut -d' ' -f1)"
}

build_pabcnet() {
    local stamp_file="$PABCNET_DIR/.tjtm-stamp"
    if [ -f "$PABCNET_DIR/bin/pabcnetc.exe" ] && [ -f "$PABCNET_DIR/bin/Lib/PABCSystem.pcu" ] &&
       [ "$(cat "$stamp_file" 2>/dev/null)" = "$(pabcnet_stamp)" ]; then
        log "PascalABC.NET уже собран: $PABCNET_DIR"
        return
    fi

    log "получаю PascalABC.NET $PABCNET_REV"
    mkdir -p "$PABCNET_DIR"
    if [ ! -d "$PABCNET_DIR/.git" ]; then
        git -C "$PABCNET_DIR" init -q
        git -C "$PABCNET_DIR" remote add origin "$PABCNET_URL"
    fi
    GIT_LFS_SKIP_SMUDGE=1 git -C "$PABCNET_DIR" fetch -q --depth 1 origin "$PABCNET_REV"
    git -C "$PABCNET_DIR" checkout -q --force FETCH_HEAD
    git -C "$PABCNET_DIR" clean -q -fdx
    git -C "$PABCNET_DIR" apply "$PABCNET_PATCH"

    log "собираю компилятор (dotnet, net472 под Mono)"
    (
        cd "$PABCNET_DIR"
        export DOTNET_CLI_TELEMETRY_OPTOUT=1 DOTNET_NOLOGO=1
        dotnet build -c Release --no-incremental pabcnetc.sln \
            -p:PABCNET_LEGACY_ONLY=true -v:q -nologo -clp:ErrorsOnly
    )

    log "собираю стандартные модули (GraphABC и другие)"
    (
        cd "$PABCNET_DIR/ReleaseGenerators"
        mono ../bin/pabcnetc.exe RebuildStandartModulesMono.pas /rebuild > /dev/null
    )
    [ -f "$PABCNET_DIR/bin/Lib/PABCSystem.pcu" ] || { log "модули не собрались"; exit 1; }
    pabcnet_stamp > "$stamp_file"
    log "PascalABC.NET готов"
}

fetch_corpus() {
    # Корпус — не обязательная часть: без сети остаётся прежняя копия,
    # а тесты, которым он нужен, пропускаются, а не падают.
    if [ -d "$ANDORS_DIR/.git" ]; then
        log "обновляю корпус Andors-Love"
        if GIT_LFS_SKIP_SMUDGE=1 git -C "$ANDORS_DIR" fetch -q --depth 1 origin HEAD; then
            git -C "$ANDORS_DIR" reset -q --hard FETCH_HEAD
        else
            log "не удалось обновить корпус, остаётся прежний"
        fi
    else
        log "получаю корпус Andors-Love"
        GIT_LFS_SKIP_SMUDGE=1 git clone -q --depth 1 "$ANDORS_URL" "$ANDORS_DIR" ||
            log "не удалось получить корпус: тесты на нём будут пропущены"
    fi
}

write_env() {
    cat > "$TOOLS_DIR/env.sh" <<EOF
# Создано tools/setup-env.sh. Пути к инструментам переноса TJTM.
export TJTM_TOOLS_DIR="$TOOLS_DIR"
export PABCNET_DIR="$PABCNET_DIR"
export ANDORS_LOVE_DIR="$ANDORS_DIR"
export CMAKE_PREFIX_PATH="$SDL_PREFIX\${CMAKE_PREFIX_PATH:+:\$CMAKE_PREFIX_PATH}"
export DOTNET_CLI_TELEMETRY_OPTOUT=1
export DOTNET_NOLOGO=1
EOF
    log "пути записаны в $TOOLS_DIR/env.sh"
}

mkdir -p "$TOOLS_DIR"
[ "$SKIP_APT" -eq 1 ] || install_packages
[ "$SKIP_SDL" -eq 1 ] || build_sdl
[ "$SKIP_PABCNET" -eq 1 ] || build_pabcnet
[ "$SKIP_CORPUS" -eq 1 ] || fetch_corpus
write_env
log "готово"
