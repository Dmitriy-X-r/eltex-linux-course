#ifndef CHAT_H
#define CHAT_H

#include <mqueue.h>
#include <signal.h>

#define CHAT_MESSAGE_SIZE 1024
#define CHAT_NAME_SIZE 256
#define CHAT_MAX_MESSAGES 10

#define CHAT_MESSAGE_PRIORITY 0
#define CHAT_EXIT_PRIORITY   10

typedef struct {
    mqd_t send_queue;
    mqd_t receive_queue;

    char queue_name_1[CHAT_NAME_SIZE];
    char queue_name_2[CHAT_NAME_SIZE];

    int is_owner;
} Chat;

int chat_init(Chat *chat, const char *base_name);
int chat_run(Chat *chat);
void chat_close(Chat *chat);

#endif
