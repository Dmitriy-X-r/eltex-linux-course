#define _POSIX_C_SOURCE 200809L

#include "chat.h"

#include <arpa/inet.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static volatile sig_atomic_t stop_requested = 0;

static void handle_sigint(int signal_number)
{
    (void)signal_number;
    stop_requested = 1;
}

static int send_message(int socket_fd,
                        uint32_t type,
                        const char *hostname,
                        const char *text)
{
    ChatMessage message = {0};

    message.type = htonl(type);

    snprintf(message.hostname, sizeof(message.hostname),
             "%s", hostname);

    if (text != NULL) {
        snprintf(message.text, sizeof(message.text),
                 "%s", text);
    }

    const char *data = (const char *)&message;
    size_t remaining = sizeof(message);

    while (remaining > 0) {
        ssize_t sent = send(socket_fd, data, remaining, MSG_NOSIGNAL);

        if (sent == -1) {
            if (errno == EINTR) {
                continue;
            }

            perror("send");
            return -1;
        }

        if (sent == 0) {
            fprintf(stderr, "Соединение закрыто при отправке.\n");
            return -1;
        }

        data += sent;
        remaining -= (size_t)sent;
    }

    return 0;
}

static void print_message(const ChatMessage *message)
{
    uint32_t type = ntohl(message->type);

    if (type == CHAT_MESSAGE_JOIN) {
        printf("\n[Сеть] Подключился участник: %s\n",
               message->hostname);
    } else if (type == CHAT_MESSAGE_TEXT) {
        printf("\n[%s] %s\n",
               message->hostname, message->text);
    } else if (type == CHAT_MESSAGE_LEAVE) {
        printf("\n[Сеть] Участник отключился: %s\n",
               message->hostname);
    }

    printf("> ");
    fflush(stdout);
}

int run_chat(const char *server_ip)
{
    int socket_fd = -1;
    int result = EXIT_FAILURE;
    int leave_sent = 0;
    int running = 1;

    struct sigaction action = {0};
    struct sigaction old_action;

    action.sa_handler = handle_sigint;
    sigemptyset(&action.sa_mask);

    if (sigaction(SIGINT, &action, &old_action) == -1) {
        perror("sigaction");
        return EXIT_FAILURE;
    }

    char hostname[CHAT_HOSTNAME_LENGTH];

    if (gethostname(hostname, sizeof(hostname)) == -1) {
        perror("gethostname");
        goto cleanup;
    }

    hostname[sizeof(hostname) - 1] = '\0';

    struct sockaddr_in server_address = {0};

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(CHAT_PORT);

    if (inet_pton(AF_INET, server_ip,
                  &server_address.sin_addr) != 1) {
        fprintf(stderr, "Некорректный IPv4-адрес: %s\n",
                server_ip);
        goto cleanup;
    }

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (socket_fd == -1) {
        perror("socket");
        goto cleanup;
    }

    if (connect(socket_fd,
                (struct sockaddr *)&server_address,
                sizeof(server_address)) == -1) {
        perror("connect");
        goto cleanup;
    }

    if (send_message(socket_fd, CHAT_MESSAGE_JOIN,
                     hostname, NULL) == -1) {
        goto cleanup;
    }

    printf("Подключено к серверу %s:%d\n",
           server_ip, CHAT_PORT);
    printf("Введите сообщение или /quit для выхода.\n");

    /*
     * Отключаем буферизацию stdin, чтобы poll() корректно
     * отслеживал ввод с клавиатуры.
     */
    setvbuf(stdin, NULL, _IONBF, 0);

    struct pollfd descriptors[2] = {
        {
            .fd = socket_fd,
            .events = POLLIN
        },
        {
            .fd = STDIN_FILENO,
            .events = POLLIN
        }
    };

    ChatMessage incoming;
    size_t received_bytes = 0;

    while (running && !stop_requested) {
        int ready = poll(descriptors, 2, 500);

        if (ready == -1) {
            if (errno == EINTR) {
                continue;
            }

            perror("poll");
            break;
        }

        if (stop_requested) {
            break;
        }

        if (ready == 0) {
            continue;
        }

        if (descriptors[0].revents & POLLIN) {
            char *destination = (char *)&incoming;
            size_t remaining = sizeof(incoming) - received_bytes;

            ssize_t count = recv(socket_fd,
                                 destination + received_bytes,
                                 remaining, 0);

            if (count == -1) {
                if (errno == EINTR || errno == EAGAIN) {
                    continue;
                }

                perror("recv");
                break;
            }

            if (count == 0) {
                printf("\nСервер закрыл соединение.\n");
                break;
            }

            received_bytes += (size_t)count;

            if (received_bytes == sizeof(incoming)) {
                incoming.hostname[CHAT_HOSTNAME_LENGTH - 1] = '\0';
                incoming.text[CHAT_TEXT_LENGTH - 1] = '\0';

                print_message(&incoming);
                received_bytes = 0;
            }
        }

        if (descriptors[0].revents & (POLLERR | POLLNVAL)) {
            fprintf(stderr, "Ошибка TCP-сокета.\n");
            break;
        }

        if (descriptors[0].revents & POLLHUP) {
            printf("\nСоединение с сервером завершено.\n");
            break;
        }

        if (descriptors[1].revents & POLLIN) {
            char input[CHAT_TEXT_LENGTH];

            if (fgets(input, sizeof(input), stdin) == NULL) {
                running = 0;
                break;
            }

            input[strcspn(input, "\n")] = '\0';

            if (strcmp(input, "/quit") == 0) {
                running = 0;
                break;
            }

            if (input[0] == '\0') {
                printf("> ");
                fflush(stdout);
                continue;
            }

            if (send_message(socket_fd, CHAT_MESSAGE_TEXT,
                             hostname, input) == -1) {
                running = 0;
                break;
            }

            printf("> ");
            fflush(stdout);
        }

        if (descriptors[1].revents & (POLLERR | POLLNVAL | POLLHUP)) {
            running = 0;
            break;
        }
    }

    /*
     * Уведомляем сервер об уходе, если соединение ещё работает.
     */
    if (socket_fd != -1 && !leave_sent) {
        if (send_message(socket_fd, CHAT_MESSAGE_LEAVE,
                         hostname, NULL) == 0) {
            leave_sent = 1;
        }
    }

    result = EXIT_SUCCESS;

cleanup:
    if (socket_fd != -1) {
        close(socket_fd);
    }

    if (sigaction(SIGINT, &old_action, NULL) == -1) {
        perror("sigaction restore");
        result = EXIT_FAILURE;
    }

    return result;
}
