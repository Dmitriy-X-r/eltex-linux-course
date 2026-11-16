#define _POSIX_C_SOURCE 200809L

#include "chat.h"

#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

static volatile sig_atomic_t interrupted = 0;

static void handle_sigint(int signal_number)
{
    (void)signal_number;
    interrupted = 1;
}

static int set_signal_handler(void)
{
    struct sigaction action = {0};

    action.sa_handler = handle_sigint;
    sigemptyset(&action.sa_mask);

    if (sigaction(SIGINT, &action, NULL) == -1) {
        perror("sigaction");
        return -1;
    }

    return 0;
}

static int prepare_names(Chat *chat, const char *base_name)
{
    if (base_name == NULL || base_name[0] == '\0'
        || strchr(base_name, '/') != NULL) {
        fprintf(stderr, "Invalid queue name\n");
        return -1;
    }

    int result = snprintf(chat->queue_name_1,
                          sizeof(chat->queue_name_1),
                          "/%s_1", base_name);

    if (result < 0 || (size_t)result >= sizeof(chat->queue_name_1)) {
        fprintf(stderr, "Queue name is too long\n");
        return -1;
    }

    result = snprintf(chat->queue_name_2,
                      sizeof(chat->queue_name_2),
                      "/%s_2", base_name);

    if (result < 0 || (size_t)result >= sizeof(chat->queue_name_2)) {
        fprintf(stderr, "Queue name is too long\n");
        return -1;
    }

    return 0;
}

static int check_queue_size(mqd_t queue)
{
    struct mq_attr attributes;

    if (mq_getattr(queue, &attributes) == -1) {
        perror("mq_getattr");
        return -1;
    }

    if (attributes.mq_msgsize > CHAT_MESSAGE_SIZE) {
        fprintf(stderr, "Queue message size is too large\n");
        return -1;
    }

    return 0;
}

static int open_existing_queues(Chat *chat)
{
    chat->receive_queue = mq_open(chat->queue_name_2, O_RDWR);

    if (chat->receive_queue == (mqd_t)-1) {
        perror("mq_open queue 2");
        return -1;
    }

    chat->send_queue = mq_open(chat->queue_name_1, O_RDWR);

    if (chat->send_queue == (mqd_t)-1) {
        perror("mq_open queue 1");
        mq_close(chat->receive_queue);
        chat->receive_queue = (mqd_t)-1;
        return -1;
    }

    if (check_queue_size(chat->receive_queue) == -1
        || check_queue_size(chat->send_queue) == -1) {
        mq_close(chat->receive_queue);
        mq_close(chat->send_queue);

        chat->receive_queue = (mqd_t)-1;
        chat->send_queue = (mqd_t)-1;

        return -1;
    }

    chat->is_owner = 0;

    return 0;
}

int chat_init(Chat *chat, const char *base_name)
{
    if (chat == NULL) {
        fprintf(stderr, "Invalid chat pointer\n");
        return -1;
    }

    chat->send_queue = (mqd_t)-1;
    chat->receive_queue = (mqd_t)-1;
    chat->is_owner = 0;

    if (prepare_names(chat, base_name) == -1) {
        return -1;
    }

    if (set_signal_handler() == -1) {
        return -1;
    }

    struct mq_attr attributes = {0};
    attributes.mq_maxmsg = CHAT_MAX_MESSAGES;
    attributes.mq_msgsize = CHAT_MESSAGE_SIZE;

    mqd_t queue_1 = mq_open(chat->queue_name_1,
                            O_CREAT | O_EXCL | O_RDWR,
                            0600, &attributes);

    if (queue_1 == (mqd_t)-1) {
        if (errno == EEXIST) {
            return open_existing_queues(chat);
        }

        perror("mq_open queue 1");
        return -1;
    }

    mqd_t queue_2 = mq_open(chat->queue_name_2,
                            O_CREAT | O_EXCL | O_RDWR,
                            0600, &attributes);

    if (queue_2 == (mqd_t)-1) {
        int saved_errno = errno;

        mq_close(queue_1);
        mq_unlink(chat->queue_name_1);

        if (saved_errno == EEXIST) {
            return open_existing_queues(chat);
        }

        errno = saved_errno;
        perror("mq_open queue 2");
        return -1;
    }

    chat->receive_queue = queue_1;
    chat->send_queue = queue_2;
    chat->is_owner = 1;

    printf("Created queues: %s and %s\n",
           chat->queue_name_1, chat->queue_name_2);

    return 0;
}

static int send_message(mqd_t queue, const char *message,
                        unsigned int priority)
{
    size_t message_size = strlen(message) + 1;

    if (mq_send(queue, message, message_size, priority) == -1) {
        perror("mq_send");
        return -1;
    }

    return 0;
}

static int receive_message(mqd_t queue, char *buffer,
                           unsigned int *priority)
{
    ssize_t received = mq_receive(queue, buffer,
                                  CHAT_MESSAGE_SIZE, priority);

    if (received == -1) {
        if (errno == EINTR) {
            return 0;
        }

        perror("mq_receive");
        return -1;
    }

    buffer[received] = '\0';

    return 1;
}

int chat_run(Chat *chat)
{
    if (chat == NULL
        || chat->send_queue == (mqd_t)-1
        || chat->receive_queue == (mqd_t)-1) {
        fprintf(stderr, "Chat is not initialized\n");
        return -1;
    }

    char message[CHAT_MESSAGE_SIZE + 1];
    int running = 1;

    printf("Chat started. Type /exit or press Ctrl+C to leave.\n");
    printf("You: ");
    fflush(stdout);

    while (running && !interrupted) {
        struct pollfd descriptors[2] = {
            {
                .fd = STDIN_FILENO,
                .events = POLLIN
            },
            {
                .fd = (int)chat->receive_queue,
                .events = POLLIN
            }
        };

        int ready = poll(descriptors, 2, -1);

        if (ready == -1) {
            if (errno == EINTR) {
                continue;
            }

            perror("poll");
            return -1;
        }

        if (descriptors[1].revents & POLLIN) {
            unsigned int priority = 0;

            int result = receive_message(chat->receive_queue,
                                         message, &priority);

            if (result == -1) {
                return -1;
            }

            if (result == 1) {
                if (priority == CHAT_EXIT_PRIORITY) {
                    printf("\nThe other participant has left the chat.\n");
                    running = 0;
                } else {
                    printf("\nPeer: %s\n", message);
                    printf("You: ");
                    fflush(stdout);
                }
            }
        }

        if (running && (descriptors[0].revents & POLLIN)) {
            if (fgets(message, sizeof(message), stdin) == NULL) {
                if (interrupted || feof(stdin)) {
                    break;
                }

                if (errno == EINTR) {
                    clearerr(stdin);
                    continue;
                }

                perror("fgets");
                return -1;
            }

            message[strcspn(message, "\n")] = '\0';

            if (strcmp(message, "/exit") == 0) {
                break;
            }

            if (send_message(chat->send_queue, message,
                             CHAT_MESSAGE_PRIORITY) == -1) {
                return -1;
            }

            printf("You: ");
            fflush(stdout);
        }
    }

    if (send_message(chat->send_queue, "CHAT_EXIT",
                     CHAT_EXIT_PRIORITY) == -1) {
        return -1;
    }

    printf("\nLeaving chat...\n");

    return 0;
}

void chat_close(Chat *chat)
{
    if (chat == NULL) {
        return;
    }

    if (chat->send_queue != (mqd_t)-1) {
        mq_close(chat->send_queue);
        chat->send_queue = (mqd_t)-1;
    }

    if (chat->receive_queue != (mqd_t)-1) {
        mq_close(chat->receive_queue);
        chat->receive_queue = (mqd_t)-1;
    }

    if (chat->is_owner) {
        if (mq_unlink(chat->queue_name_1) == -1 && errno != ENOENT) {
            perror("mq_unlink queue 1");
        }

        if (mq_unlink(chat->queue_name_2) == -1 && errno != ENOENT) {
            perror("mq_unlink queue 2");
        }

        printf("Queues removed.\n");
    }
}
