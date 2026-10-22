#!/bin/bash

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
CONFIG_FILE="$SCRIPT_DIR/observer.conf"
OBSERVER_LOG="$SCRIPT_DIR/observer.log"

if [[ ! -f "$CONFIG_FILE" ]]; then
    echo "Ошибка: файл конфигурации не найден: $CONFIG_FILE" >&2
    exit 1
fi

while IFS= read -r SCRIPT_NAME || [[ -n "$SCRIPT_NAME" ]]; do
    SCRIPT_NAME="${SCRIPT_NAME%%#*}"
    SCRIPT_NAME="$(echo "$SCRIPT_NAME" | xargs)"

    [[ -z "$SCRIPT_NAME" ]] && continue

    SCRIPT_PATH="$SCRIPT_DIR/$SCRIPT_NAME"

    if [[ ! -x "$SCRIPT_PATH" ]]; then
        printf '%s Ошибка: скрипт %s отсутствует или не исполняемый\n' \
            "$(date '+%Y-%m-%d %H:%M:%S')" "$SCRIPT_NAME" >> "$OBSERVER_LOG"
        continue
    fi

    RUNNING=0

    for PROC_FILE in /proc/[0-9]*/cmdline; do
        [[ -r "$PROC_FILE" ]] || continue

        CMDLINE="$(tr '\0' ' ' < "$PROC_FILE" 2>/dev/null)"

        if [[ "$CMDLINE" == *"$SCRIPT_PATH"* ]]; then
            RUNNING=1
            break
        fi
    done

    if [[ "$RUNNING" -eq 0 ]]; then
        nohup "$SCRIPT_PATH" </dev/null >/dev/null 2>&1 &

        printf '%s Запущен скрипт %s, PID %s\n' \
            "$(date '+%Y-%m-%d %H:%M:%S')" \
            "$SCRIPT_NAME" "$!" >> "$OBSERVER_LOG"
    fi
done < "$CONFIG_FILE"
