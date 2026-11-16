#include "chat.h"

#include <stdio.h>

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <queue_name>\n", argv[0]);
        return 1;
    }

    Chat chat;

    if (chat_init(&chat, argv[1]) == -1) {
        fprintf(stderr, "Failed to initialize chat\n");
        return 1;
    }

    int result = chat_run(&chat);

    chat_close(&chat);

    return result == 0 ? 0 : 1;
}
