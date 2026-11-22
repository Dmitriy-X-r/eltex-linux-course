#ifndef CHAT_H
#define CHAT_H

#include <stdint.h>

#define CHAT_PORT 45454
#define CHAT_HOSTNAME_LENGTH 64
#define CHAT_TEXT_LENGTH 512

typedef enum {
    CHAT_MESSAGE_JOIN = 1,
    CHAT_MESSAGE_TEXT,
    CHAT_MESSAGE_LEAVE
} ChatMessageType;

typedef struct {
    uint32_t type;
    char hostname[CHAT_HOSTNAME_LENGTH];
    char text[CHAT_TEXT_LENGTH];
} ChatMessage;

int run_chat(const char *server_ip);
int run_server(void);

#endif
