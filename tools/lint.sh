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
# Файлы проверяются параллельно (run-clang-tidy): по одному проверка всего
# проекта шла десяток минут.
# Замечания собираются в переменную, а не проверяются концом конвейера:
# найдя ошибку, clang-tidy сам выходит с ненулевым кодом, и при pipefail
# «grep && status=1» тогда не срабатывал — ошибки печатались, а итогом
# было «замечаний нет».
if [ "${#SOURCES[@]}" -gt 0 ]; then
    if command -v run-clang-tidy >/dev/null 2>&1; then
        tidy=$(run-clang-tidy -p build/debug -quiet -j "$(nproc)" "${SOURCES[@]}" 2>/dev/null |
            grep -E "^/.*(warning|error):" | sort -u)
    else
        tidy=$(clang-tidy -p build/debug --quiet "${SOURCES[@]}" 2>/dev/null |
            grep -E "warning:|error:")
    fi
    if [ -n "$tidy" ]; then
        echo "$tidy"
        status=1
    fi
fi

echo "== cppcheck"
# useStlAlgorithm — совет заменить цикл алгоритмом, это вкус; unusedStructMember
# ложно срабатывает на полях, которыми пользуются другие файлы.
# Макросы GoogleTest (TEST_P, INSTANTIATE_TEST_SUITE_P) cppcheck понимает
# только с их описанием, иначе видит в тестах синтаксическую ошибку.
cppcheck --enable=warning,style,performance,portability --std=c++20 --language=c++ --quiet \
    --library=googletest --inline-suppr --error-exitcode=1 \
    --suppress=missingIncludeSystem --suppress=useStlAlgorithm --suppress=unusedStructMember \
    -I src "${FILES[@]}" || status=1

echo "== цвета интерфейса"
# Цвет экрана — роль темы (src/core/theme.h), а не число: иначе новые
# экраны снова рождаются с литералами, и тема их не перекрасит
# (docs/ui-design-system.md, шаг 2). Цвета документа — kClear, kBlack, kWhite.
literals=$(grep -nE '\ba?rgb\(\s*[0-9]+\s*(,\s*[0-9]+\s*){2,3}\)' src/app/*.cpp src/app/*.h)
if [ -n "$literals" ]; then
    echo "$literals"
    echo "lint: цвет числом в src/app — заведите роль в src/core/theme.h" >&2
    status=1
fi

echo "== shellcheck"
# Скрипты проверяются всегда целиком: их немного, и они ссылаются друг на друга.
# Локаль UTF-8 обязательна: без неё проверяльщик спотыкается о кириллицу.
LC_ALL=C.UTF-8 shellcheck -x tools/*.sh .claude/hooks/*.sh || status=1

if [ "$status" -eq 0 ]; then echo "замечаний нет"; fi
exit "$status"
