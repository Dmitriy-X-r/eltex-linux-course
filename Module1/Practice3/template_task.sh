#!/bin/bash

SCRIPT_NAME="$(basename -- "$0")"

if [[ "$SCRIPT_NAME" == "template_task.sh" ]]; then
    echo "я бригадир, сам не работаю"
    exit 1
fi

SCRIPT_DIR="$(cd -- "$(dirname -- "$0")" && pwd)"
REPORT_FILE="$SCRIPT_DIR/report_${SCRIPT_NAME}.log"
FIFO_PATH="/root/task3_part2/cuckoo.fifo"
PID="$$"

printf '%s [%s] Скрипт запущен\n' \
    "$(date '+%Y-%m-%d %H:%M:%S')" "$PID" >> "$REPORT_FILE"

REPLY_FIFO="/tmp/cuckoo_reply_${PID}.fifo"
mkfifo -m 0600 "$REPLY_FIFO" || exit 1

cleanup() {
    rm -f -- "$REPLY_FIFO"
}
trap cleanup EXIT

NAME="${SCRIPT_NAME}"

printf '%s[%s]: how much time do I have?\n' "$NAME" "$PID" > "$FIFO_PATH"

if ! read -r N < "$REPLY_FIFO"; then
    echo "Не удалось получить ответ от cuckoo.sh" >&2
    exit 1
fi

if [[ ! "$N" =~ ^([2-9]|10)$ ]]; then
    echo "Получен некорректный ответ: $N" >&2
    exit 1
fi

sleep "$N"

printf '%s [%s] Скрипт завершился, работал %s секунд.\n' \
    "$(date '+%Y-%m-%d %H:%M:%S')" "$PID" "$N" >> "$REPORT_FILE"
