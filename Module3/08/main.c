#include "capture.h"

#include <stdio.h>
#include <stdlib.h>

int main(void){
    int filter;
    int duration;

    printf("Выберите фильтр:\n");
    printf("1. UDP-чат (порт 45454)\n");
    printf("2. DNS (UDP, порт 53)\n");
    printf("Ваш выбор: ");

    if (scanf("%d", &filter) != 1 ||
        (filter != 1 && filter != 2)) {
        fprintf(stderr, "Некорректный выбор фильтра.\n");
        return EXIT_FAILURE;
    }

    printf("Длительность захвата в секундах: ");

    if (scanf("%d", &duration) != 1 ||
        duration < 1 || duration > 3600) {
        fprintf(stderr,
                "Введите длительность от 1 до 3600 секунд.\n");
        return EXIT_FAILURE;
    }

    return run_capture(filter, duration);
}