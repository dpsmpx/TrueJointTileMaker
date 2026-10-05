#!/bin/bash
# Проверка кода переноса: для C++ — оформление, clang-tidy и cppcheck,
# для скриптов в tools/ — их собственный проверяльщик.
#
#   tools/lint.sh            проверить src/, tests/ и tools/
#   tools/lint.sh --fix      заодно поправить оформление clang-format
#   tools/lint.sh FILE...    проверить только эти файлы
#
# clang-tidy берёт флаги сборки из build/debug/compile_commands.json,
# поэтому сначала нужна сборка: cmake --preset debug.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT" || exit 1

FIX=0
FILES=()
for arg in "$@"; do
    case "$arg" in
        --fix) FIX=1 ;;
        *) FILES+=("$arg") ;;
    esac
done
if [ "${#FILES[@]}" -eq 0 ]; then
    mapfile -t FILES < <(find src tests -name '*.cpp' -o -name '*.h' -o -name '*.hpp' | sort)
fi

DB="build/debug/compile_commands.json"
if [ ! -f "$DB" ]; then
    cmake --preset debug >/dev/null || { echo "lint: не настроить сборку debug" >&2; exit 1; }
fi

status=0

echo "== clang-format"
if [ "$FIX" -eq 1 ]; then
    clang-format -i "${FILES[@]}"
else
    clang-format --dry-run --Werror "${FILES[@]}" || status=1
fi

echo "== clang-tidy"
# Заголовки проверяются в составе включающих их .cpp.
SOURCES=()
for f in "${FILES[@]}"; do
    case "$f" in *.cpp) SOURCES+=("$f") ;; esac
done
if [ "${#SOURCES[@]}" -gt 0 ]; then
    clang-tidy -p build/debug --quiet "${SOURCES[@]}" 2>/dev/null | grep -E "warning:|error:" && status=1
fi

echo "== cppcheck"
# useStlAlgorithm — совет заменить цикл алгоритмом; это вкус, а не ошибка.
cppcheck --enable=warning,style,performance,portability --std=c++17 --quiet \
    --inline-suppr --error-exitcode=1 \
    --suppress=missingIncludeSystem --suppress=useStlAlgorithm \
    -I src "${FILES[@]}" || status=1

echo "== shellcheck"
# Скрипты проверяются всегда целиком: их немного, и они ссылаются друг на друга.
# Локаль UTF-8 обязательна: без неё проверяльщик спотыкается о кириллицу.
LC_ALL=C.UTF-8 shellcheck -x tools/*.sh .claude/hooks/*.sh || status=1

if [ "$status" -eq 0 ]; then echo "замечаний нет"; fi
exit "$status"
