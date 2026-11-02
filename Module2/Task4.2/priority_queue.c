#include <stdlib.h>
#include "priority_queue.h"

void queue_init(PriorityQueue *queue) {
    if(queue == NULL) {
        return;
    }

    queue->head = NULL;
    queue->tail = NULL;
}

int queue_enqueue(PriorityQueue *queue, const Message *message) {
    if(queue == NULL || message == NULL) {
        return 0;
    }

    if (message->priority < MIN_PRIORITY ||
        message->priority > MAX_PRIORITY) {
        return 0;
    }

    QueueNode *new_node = malloc(sizeof(QueueNode));

    if(new_node == NULL) {
        return 0;
    }

    new_node->message = *message;
    new_node->next = NULL;

    if(queue->tail == NULL) {
        queue->head = new_node;
        queue->tail = new_node;
    } else {
        queue->tail->next = new_node;
        queue->tail = new_node;
    }

    return 1;
}

int queue_dequeue_first(PriorityQueue *queue, Message *result) {
    if (queue == NULL || result == NULL || queue->head == NULL) {
        return 0;
    }

    QueueNode *first = queue->head;

    *result = first->message;
    queue->head = first->next;

    if (queue->head == NULL) {
        queue->tail = NULL;
    }

    free(first);

    return 1;
}

static int queue_remove_node(PriorityQueue *queue, QueueNode *previous, QueueNode *current, Message *result) {
    if (queue == NULL || current == NULL || result == NULL) {
        return 0;
    }

    *result = current->message;

    if (previous == NULL) {
        queue->head = current->next;
    } else {
        previous->next = current->next;
    }

    if (queue->tail == current) {
        queue->tail = previous;
    }

    free(current);

    return 1;
}

int queue_dequeue_priority(PriorityQueue *queue, int priority, Message *result)
{
    if (queue == NULL || result == NULL || priority < MIN_PRIORITY || priority > MAX_PRIORITY) {
        return 0;
    }

    QueueNode *previous = NULL;
    QueueNode *current = queue->head;

    while (current != NULL) {
        if (current->message.priority == priority) {
            return queue_remove_node(queue, previous, current, result);
        }

        previous = current;
        current = current->next;
    }

    return 0;
}

int queue_dequeue_at_least(PriorityQueue *queue, int priority, Message *result){
    if (queue == NULL || result == NULL || priority < MIN_PRIORITY || priority > MAX_PRIORITY) {
        return 0;
    }

    QueueNode *previous = NULL;
    QueueNode *current = queue->head;

    QueueNode *best_node = NULL;
    QueueNode *best_previous = NULL;
    int best_priority = -1;

    while (current != NULL) {
        int current_priority = current->message.priority;

        if (current_priority <= priority && current_priority > best_priority) {
            best_priority = current_priority;
            best_node = current;
            best_previous = previous;
        }

        previous = current;
        current = current->next;
    }

    if (best_node == NULL) {
        return 0;
    }

    return queue_remove_node(queue, best_previous, best_node, result);
}

void queue_clear(PriorityQueue *queue)
{
    if (queue == NULL) {
        return;
    }

    QueueNode *current = queue->head;

    while (current != NULL) {
        QueueNode *next = current->next;

        free(current);

        current = next;
    }

    queue->head = NULL;
    queue->tail = NULL;
}