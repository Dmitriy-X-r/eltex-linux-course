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

static void handle_sigint(int signal_number) {
    (void)signal_number;
    stop_requested = 1;
}


static int send_packet(int socket_fd, const ChatMessage *message) {
    const char *data = (const char *)message;
    size_t remaining = sizeof(*message);

    while (remaining > 0) {
        ssize_t sent = send(socket_fd, data, remaining, MSG_NOSIGNAL);

        if (sent == -1) {
            if (errno == EINTR)
                continue;

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



static int send_message(int socket_fd, uint32_t type, const char *hostname, const char *text) {
    ChatMessage message = {0};

    message.type = htonl(type);
    message.data_length = htonl(0);

    snprintf(message.hostname, sizeof(message.hostname),
             "%s", hostname);

    if (text != NULL) {
        snprintf(message.text, sizeof(message.text),
                 "%s", text);
    }

    return send_packet(socket_fd, &message);
}


static int send_next_file_part(int socket_fd,
                               FILE **file_to_send,
                               const char *hostname,
                               const char *filename,
                               int *start_pending)
{
    ChatMessage message = {0};

    snprintf(message.hostname, sizeof(message.hostname),
             "%s", hostname);

    if (*start_pending) {
        message.type = htonl(CHAT_MESSAGE_FILE_START);

        snprintf(message.text, sizeof(message.text),
                 "%s", filename);

        if (send_packet(socket_fd, &message) == -1)
            return -1;

        *start_pending = 0;
        return 0;
    }

    size_t count = fread(message.text, 1,
                         sizeof(message.text), *file_to_send);

    if (count > 0) {
        message.type = htonl(CHAT_MESSAGE_FILE_DATA);
        message.data_length = htonl((uint32_t)count);

        if (send_packet(socket_fd, &message) == -1)
            return -1;

        return 0;
    }

    if (ferror(*file_to_send)) {
        perror("fread");
        return -1;
    }

    message.type = htonl(CHAT_MESSAGE_FILE_END);

    if (send_packet(socket_fd, &message) == -1)
        return -1;

    fclose(*file_to_send);
    *file_to_send = NULL;

    printf("\nФайл %s отправлен.\n", filename);
    printf("> ");
    fflush(stdout);

    return 0;
}



static void print_message(const ChatMessage *message) {
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


static void handle_file_message(const ChatMessage *message,
                                FILE **received_file,
                                char *received_path,
                                size_t path_size)
{
    uint32_t type = ntohl(message->type);

    if (type == CHAT_MESSAGE_FILE_START) {
        if (*received_file != NULL) {
            fclose(*received_file);
            *received_file = NULL;
        }

        const char *filename = strrchr(message->text, '/');
        filename = filename != NULL ? filename + 1 : message->text;

        if (filename[0] == '\0')
            filename = "file.bin";

        snprintf(received_path, path_size,
                 "received_%s", filename);

        *received_file = fopen(received_path, "wb");

        if (*received_file == NULL) {
            perror("fopen received file");
            received_path[0] = '\0';
            return;
        }

        printf("\nНачинается получение файла: %s\n", received_path);
        return;
    }

    if (type == CHAT_MESSAGE_FILE_DATA) {
        uint32_t length = ntohl(message->data_length);

        if (length > sizeof(message->text)) {
            fprintf(stderr, "Некорректный размер блока файла.\n");
            return;
        }

        if (*received_file == NULL) {
            fprintf(stderr, "Блок файла получен без FILE_START.\n");
            return;
        }

        if (fwrite(message->text, 1, length,
                   *received_file) != length) {
            perror("fwrite");
            fclose(*received_file);
            *received_file = NULL;
        }

        return;
    }

    if (type == CHAT_MESSAGE_FILE_END) {
        if (*received_file != NULL) {
            fclose(*received_file);
            *received_file = NULL;
            printf("\nФайл сохранён: %s\n", received_path);
            received_path[0] = '\0';
        }
    }
}


int run_chat(const char *server_ip) {
    int socket_fd = -1;
    int result = EXIT_FAILURE;
    int running = 1;
    FILE *file_to_send = NULL;
    FILE *received_file = NULL;

    char outgoing_filename[CHAT_TEXT_LENGTH] = {0};
    char received_path[CHAT_TEXT_LENGTH + 16] = {0};

    int file_start_pending = 0;

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
        descriptors[0].events = POLLIN;

        if (file_to_send != NULL) {
            descriptors[0].events |= POLLOUT;
        }
            
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
                uint32_t type = ntohl(incoming.type);

                incoming.hostname[CHAT_HOSTNAME_LENGTH - 1] = '\0';

                if (type != CHAT_MESSAGE_FILE_DATA)
                    incoming.text[CHAT_TEXT_LENGTH - 1] = '\0';

                if (type == CHAT_MESSAGE_FILE_START ||
                    type == CHAT_MESSAGE_FILE_DATA ||
                    type == CHAT_MESSAGE_FILE_END) {
                    handle_file_message(&incoming, &received_file,
                                        received_path, sizeof(received_path));
                } else {
                    print_message(&incoming);
                }

                received_bytes = 0;
            }
        }

        if (descriptors[0].revents & POLLOUT) {
            if (file_to_send != NULL) {
                if (send_next_file_part(socket_fd, &file_to_send,
                                        hostname, outgoing_filename,
                                        &file_start_pending) == -1) {
                    fprintf(stderr, "Ошибка отправки файла.\n");
                    fclose(file_to_send);
                    file_to_send = NULL;
                    file_start_pending = 0;
                }
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

            if (strncmp(input, "/send ", 6) == 0) {
                const char *path = input + 6;

                if (file_to_send != NULL) {
                    printf("Дождитесь завершения предыдущей передачи.\n");
                    printf("> ");
                    fflush(stdout);
                    continue;
                }

                FILE *opened = fopen(path, "rb");

                if (opened == NULL) {
                    perror("fopen file to send");
                    printf("> ");
                    fflush(stdout);
                    continue;
                }

                const char *filename = strrchr(path, '/');
                filename = filename != NULL ? filename + 1 : path;

                if (filename[0] == '\0') {
                    fprintf(stderr, "Не удалось определить имя файла.\n");
                    fclose(opened);
                    printf("> ");
                    fflush(stdout);
                    continue;
                }

                snprintf(outgoing_filename, sizeof(outgoing_filename),
                        "%s", filename);

                file_to_send = opened;
                file_start_pending = 1;

                printf("Подготовлен файл к отправке: %s\n",
                    outgoing_filename);
                continue;
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

    if (socket_fd != -1) {
        send_message(socket_fd, CHAT_MESSAGE_LEAVE, hostname, NULL);
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

    if (file_to_send != NULL) {
        fclose(file_to_send);
    }

    if (received_file != NULL){
        fclose(received_file);
    }

    return result;
}
