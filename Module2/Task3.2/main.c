
#include "subnet.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int run_simulation()
{
    char gateway_text[16];
    char mask_text[16];
    char count_text[32];

    uint32_t gateway_ip;
    uint32_t mask;

    printf("\nВведите IP-адрес шлюза: ");

    if (scanf("%15s", gateway_text) != 1) {
        fprintf(stderr, "Ошибка: не удалось прочитать IP-адрес шлюза.\n");
        return 0;
    }

    if (!parse_ipv4(gateway_text, &gateway_ip)) {
        fprintf(stderr, "Ошибка: некорректный IP-адрес шлюза.\n");
        return 0;
    }

    printf("Введите маску подсети: ");

    if (scanf("%15s", mask_text) != 1) {
        fprintf(stderr, "Ошибка: не удалось прочитать маску подсети.\n");
        return 0;
    }

    if (!parse_ipv4(mask_text, &mask)) {
        fprintf(stderr, "Ошибка: некорректная запись маски.\n");
        return 0;
    }

    if (!is_valid_netmask(mask)) {
        fprintf(stderr, "Ошибка: маска подсети некорректна.\n");
        return 0;
    }

    printf("Введите количество пакетов: ");

    if (scanf("%31s", count_text) != 1) {
        fprintf(stderr, "Ошибка: не удалось прочитать количество пакетов.\n");
        return 0;
    }

    char *end = NULL;
    errno = 0;

    unsigned long long count = strtoull(count_text, &end, 10);

    if (errno == ERANGE ||
        end == count_text ||
        *end != '\0' ||
        count_text[0] == '-' ||
        count == 0 ||
        count > (unsigned long long)SIZE_MAX) {
            fprintf(stderr, "Ошибка: количество пакетов должно быть положительным целым числом.\n");
            return 0;
    }

    size_t packet_count = (size_t)count;

    PacketStats stats = simulate_packets(gateway_ip, mask, packet_count);

    double local_percent =
        (double)stats.local_packets / (double)packet_count * 100.0;

    double remote_percent =
        (double)stats.remote_packets / (double)packet_count * 100.0;

    printf("\n=== Статистика обработки пакетов ===\n");
    printf("Всего пакетов: %zu\n", packet_count);
    printf("Своя подсеть:  %zu (%.2f%%)\n",
           stats.local_packets, local_percent);
    printf("Другие сети:   %zu (%.2f%%)\n",
           stats.remote_packets, remote_percent);

    return 1;
}

int main(void)
{
    char choice[16];

    srand((unsigned int)time(NULL));

    while (1) {
        printf("\n=== Меню ===\n");
        printf("1 - Начать симуляцию\n");
        printf("q - Выход\n");
        printf("Ваш выбор: ");

        if (scanf("%15s", choice) != 1) {
            printf("\nЗавершение программы.\n");
            return 0;
        }

        if (strcmp(choice, "q") == 0) {
            printf("Завершение программы.\n");
            return 0;
        }

        if (strcmp(choice, "1") != 0) {
            printf("Неизвестная команда. Завершение программы.\n");
            return 0;
        }

        run_simulation();
    }
}
