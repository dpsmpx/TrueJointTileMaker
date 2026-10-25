#!/bin/bash
# Пакеты для CI (.github/workflows): только сборка и тесты, без эталона.
#
#   tools/ci-deps.sh linux   сборка для Linux, тесты, линтеры
#   tools/ci-deps.sh mingw   кросс-сборка для Windows и тесты под Wine
#
# Полный набор разработчика ставит tools/setup-env.sh; он тянет ещё Mono,
# .NET и PascalABC.NET для сверки с оригиналом, а CI сверяется с эталонами,
# уже лежащими в репозитории. Список Linux — подмножество setup-env.sh:
# те же версии библиотек, иначе снимки экрана разошлись бы с локальными.
set -euo pipefail

COMMON=(build-essential cmake ninja-build pkg-config git python3)
LINUX=(
    clang clang-tidy clang-format cppcheck shellcheck
    # SDL3 собирается tools/setup-env.sh из исходников: X11, Wayland,
    # ввод, D-Bus; FreeType и HarfBuzz для SDL3_ttf; PNG и JPEG
    libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxss-dev
    libxfixes-dev libxtst-dev libxkbcommon-dev libwayland-dev wayland-protocols
    libegl-dev libgl-dev libdbus-1-dev libudev-dev
    libfreetype-dev libharfbuzz-dev libpng-dev libjpeg-dev zlib1g-dev
    libgtest-dev libgmock-dev
)
MINGW=(mingw-w64 wine64 wine)

case "${1:-}" in
    linux) PACKAGES=("${COMMON[@]}" "${LINUX[@]}") ;;
    mingw) PACKAGES=("${COMMON[@]}" "${MINGW[@]}") ;;
    *) sed -n '2,5p' "$0" >&2; exit 2 ;;
esac

sudo apt-get update -q
sudo env DEBIAN_FRONTEND=noninteractive \
    apt-get install -y -q --no-install-recommends "${PACKAGES[@]}"
