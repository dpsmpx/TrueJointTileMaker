#!/bin/bash
# SessionStart-хук облачной сессии Claude Code: ставит инструменты переноса
# и собирает отладочную сборку, чтобы тесты и линтеры работали сразу.
#
# Вся установка — в tools/setup-env.sh; здесь только вызов и передача путей
# в окружение сессии. Локально хук ничего не делает: свою машину человек
# настраивает сам, тем же скриптом.
set -euo pipefail

if [ "${CLAUDE_CODE_REMOTE:-}" != "true" ]; then
    exit 0
fi

cd "${CLAUDE_PROJECT_DIR:-$(dirname "$0")/../..}"

tools/setup-env.sh

TOOLS_DIR="${TJTM_TOOLS_DIR:-$HOME/.local/share/tjtm-tools}"
if [ -n "${CLAUDE_ENV_FILE:-}" ]; then
    cat "$TOOLS_DIR/env.sh" >> "$CLAUDE_ENV_FILE"
fi

# Сломанная сборка на ветке не должна оставлять сессию без инструментов:
# о ней сообщаем, но хук не валим.
if ! { cmake --preset debug >/dev/null && cmake --build --preset debug >/dev/null; }; then
    echo "session-start: отладочная сборка не удалась, см. cmake --build --preset debug" >&2
fi
