#ifndef MESSAGE_H
#define MESSAGE_H

#include <sys/types.h>

#define QUEUE_KEY 0x454C54
#define MESSAGE_SIZE 1024
#define TOPIC_LENGTH 100

#define BROKER_MESSAGE_TYPE 1

typedef struct {
    long mtype;
    char mtext[MESSAGE_SIZE];
} Message;

#endif
