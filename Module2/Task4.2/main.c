#include <stdio.h>
#include <string.h>

#include "priority_queue.h"

static void print_queue(const PriorityQueue *queue)
{
    if (queue == NULL) {
        return;
    }

    const QueueNode *current = queue->head;
    int number = 1;

    printf("\n=== Текущая очередь ===\n");

    if (current == NULL) {
        printf("Очередь пуста.\n");
        return;
    }

    while (current != NULL) {
        printf("%d. %s (приоритет: %d)\n",
               number,
               current->message.text,
               current->message.priority);

        current = current->next;
        number++;
    }
}

static int add_message(PriorityQueue *queue, const char *text, int priority) {
    Message message = { .priority = priority };

    snprintf(message.text, sizeof(message.text), "%s", text);

    if (!queue_enqueue(queue, &message)) {
        printf("Ошибка добавления сообщения \"%s\".\n", text);
        return 0;
    }

    printf("Добавлено: %s (приоритет: %d)\n", text, priority);

    return 1;
}

static void print_result(int success, const Message *message) {
    if (success) {
        printf("Извлечено: %s (приоритет: %d)\n", message->text, message->priority);
    } else {
        printf("Подходящее сообщение не найдено.\n");
    }
}

int main(void) {
    PriorityQueue queue;
    Message result;
    int choice;
    int priority;

    queue_init(&queue);

    printf("=== Генерация сообщений ===\n");

    if (!add_message(&queue, "Сообщение A", 20) ||
        !add_message(&queue, "Сообщение B", 5) ||
        !add_message(&queue, "Сообщение C", 8) ||
        !add_message(&queue, "Сообщение D", 9) ||
        !add_message(&queue, "Сообщение E", 5) ||
        !add_message(&queue, "Сообщение F", 12) ||
        !add_message(&queue, "Сообщение G", 0)) {
        queue_clear(&queue);
        return 1;
    }

    while (1) {
        print_queue(&queue);

        printf("\n=== Меню извлечения ===\n");
        printf("1. Извлечь первое сообщение\n");
        printf("2. Извлечь сообщение с точным приоритетом\n");
        printf("3. Извлечь сообщение с приоритетом не ниже заданного\n");
        printf("0. Выход\n");
        printf("Ваш выбор: ");

        if (scanf("%d", &choice) != 1) {
            printf("\nНекорректный ввод. Завершение программы.\n");
            break;
        }

        if (choice == 0) {
            break;
        }

        switch (choice) {
            case 1: {
                int success = queue_dequeue_first(&queue, &result);
                print_result(success, &result);
                break;
            }

            case 2: {
                printf("Введите точный приоритет (%d-%d): ", MIN_PRIORITY, MAX_PRIORITY);

                if (scanf("%d", &priority) != 1) {
                    printf("\nНекорректный ввод. Завершение программы.\n");
                    queue_clear(&queue);
                    return 1;
                }

                int success = queue_dequeue_priority(&queue, priority, &result);

                print_result(success, &result);
                break;
            }

            case 3: {
                printf("Введите приоритет (%d-%d): ", MIN_PRIORITY, MAX_PRIORITY);

                if (scanf("%d", &priority) != 1) {
                    printf("\nНекорректный ввод. Завершение программы.\n");
                    queue_clear(&queue);
                    return 1;
                }

                int success = queue_dequeue_at_least(&queue, priority, &result);

                print_result(success, &result);
                break;
            }

            default:
                printf("Неизвестный пункт меню. Выберите 0, 1, 2 или 3.\n");
                break;
        }
    }

    queue_clear(&queue);
    printf("Очередь очищена. Программа завершена.\n");

    return 0;
}