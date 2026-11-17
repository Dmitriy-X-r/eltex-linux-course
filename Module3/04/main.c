#include <stdio.h>
#include <string.h>

#include "shared.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr,
                "Использование:\n"
                "  %s producer\n"
                "  %s consumer\n",
                argv[0], argv[0]);

        return 1;
    }

    if (strcmp(argv[1], "producer") == 0) {
        return producer_run();
    }

    if (strcmp(argv[1], "consumer") == 0) {
        return consumer_run();
    }

    fprintf(stderr, "Неизвестный режим: %s\n", argv[1]);

    return 1;
}
