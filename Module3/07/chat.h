#ifndef CHAT_H
#define CHAT_H

#include <stdint.h>

#define CHAT_PORT 45454
#define CHAT_HOSTNAME_LENGTH 64
#define CHAT_TEXT_LENGTH 512

typedef enum {
    CHAT_MESSAGE_JOIN = 1,
    CHAT_MESSAGE_TEXT,
    CHAT_MESSAGE_LEAVE,
    CHAT_MESSAGE_FILE_START,
    CHAT_MESSAGE_FILE_DATA,
    CHAT_MESSAGE_FILE_END
} ChatMessageType;

typedef struct {
    uint32_t type;
    char hostname[CHAT_HOSTNAME_LENGTH];
    char text[CHAT_TEXT_LENGTH];
    uint32_t data_length;
} ChatMessage;

int run_chat(const char *server_ip);
int run_server(void);

#endif
