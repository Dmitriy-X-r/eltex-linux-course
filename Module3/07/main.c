#include "chat.h"

#include <stdio.h>

int main(int argc, char *argv[])
{
    if (argc > 2) {
        fprintf(stderr, "Использование: %s [IP-сервера]\n",
                argv[0]);
        return 1;
    }

    const char *server_ip = "127.0.0.1";

    if (argc == 2) {
        server_ip = argv[1];
    }

    return run_chat(server_ip);
}
