#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H

#define TEXT_LENGTH 100
#define MIN_PRIORITY 0
#define MAX_PRIORITY 255

typedef struct {
    int priority;
    char text[TEXT_LENGTH];
} Message;

typedef struct QueueNode {
    Message message;
    struct QueueNode *next;
} QueueNode;

typedef struct {
    QueueNode *head;
    QueueNode *tail;
} PriorityQueue;

void queue_init(PriorityQueue *queue);

int queue_enqueue(PriorityQueue *queue, const Message *message);

int queue_dequeue_first(PriorityQueue *queue, Message *result);

int queue_dequeue_priority(PriorityQueue *queue, int priority, Message *result);

int queue_dequeue_at_least(PriorityQueue *queue, int priority, Message *result);

void queue_clear(PriorityQueue *queue);

#endif