#!/usr/bin/env bash
# Запустить команду, а при ошибке вынести ключевые строки её вывода в аннотацию GitHub:
# логи Actions видны только после входа в аккаунт, аннотации - всем.
# Использование: run-annotated.sh <заголовок> <команда> [аргументы...]
set -u
title="$1"
shift

log="$(mktemp)"
"$@" 2>&1 | tee "$log"
status=${PIPESTATUS[0]}

if [ "$status" -ne 0 ]; then
    {
        grep -E -i "error|fail|warn|Loc: \[|Actual|Expected" "$log" | tail -n 40
        echo "----- последние строки -----"
        tail -n 20 "$log"
    } > "$log.summary"
    # Переводы строк внутри одной аннотации кодируются как %0A.
    message="$(sed -e 's/%/%25/g' -e 's/\r//g' "$log.summary" | awk 'BEGIN { ORS = "%0A" } { print }')"
    echo "::error title=${title}::${message}"
fi

exit "$status"
