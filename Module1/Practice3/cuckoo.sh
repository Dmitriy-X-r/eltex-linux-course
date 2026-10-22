#!/bin/bash

set -u

if [[ $# -ne 1 || "$1" != --pishitesuda=* ]]; then
    echo "Использование: $0 --pishitesuda=/абсолютный/путь/к/fifo" >&2
    exit 1
fi

FIFO_PATH="${1#--pishitesuda=}"

if [[ "$FIFO_PATH" != /* || "$FIFO_PATH" == "/" ]]; then
    echo "Ошибка: необходимо указать абсолютный путь к FIFO." >&2
    exit 1
fi

SCRIPT_DIR="$(cd -- "$(dirname -- "$0")" && pwd)"
LOG_FILE="$SCRIPT_DIR/cuckoo.log"

if [[ -e "$FIFO_PATH" || -L "$FIFO_PATH" ]]; then
    echo "Ошибка: путь уже существует: $FIFO_PATH" >&2
    exit 1
fi

if ! mkfifo -m 0666 -- "$FIFO_PATH"; then
    echo "Ошибка: не удалось создать именованный канал." >&2
    exit 1
fi

shutdown() {
    trap - TERM INT
    printf '%s Shutdown!\n' "$(date '+%Y-%m-%d %H:%M:%S')" >> "$LOG_FILE"
    rm -f -- "$FIFO_PATH"
    exit 0
}

trap shutdown TERM INT

printf '%s Startup!\n' "$(date '+%Y-%m-%d %H:%M:%S')" >> "$LOG_FILE"

while true; do
    if IFS= read -r request < "$FIFO_PATH"; then
        if [[ "$request" =~ ^([^[]+)\[([0-9]+)\]:[[:space:]]how[[:space:]]much[[:space:]]time[[:space:]]do[[:space:]]I[[:space:]]have\?$ ]]; then
            NAME="${BASH_REMATCH[1]}"
            PID="${BASH_REMATCH[2]}"
            N=$(( RANDOM % 9 + 2 ))
            REPLY_FIFO="/tmp/cuckoo_reply_${PID}.fifo"

            printf '%s %s[%s] %s\n' \
                "$(date '+%Y-%m-%d %H:%M:%S')" "$NAME" "$PID" "$N" >> "$LOG_FILE"

            if [[ -p "$REPLY_FIFO" ]]; then
                printf '%s\n' "$N" > "$REPLY_FIFO"
            fi
        fi
    fi
done
