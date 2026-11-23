#define _DEFAULT_SOURCE

#include "chat.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_EVENTS 64
#define OUTPUT_QUEUE_SIZE 64

typedef struct Client {
    int fd;
    int joined;
    char hostname[CHAT_HOSTNAME_LENGTH];

    ChatMessage incoming;
    size_t received_bytes;

    ChatMessage output_queue[OUTPUT_QUEUE_SIZE];
    size_t queue_head;
    size_t queue_count;
    size_t sent_bytes;

    struct Client *next;
} Client;

static volatile sig_atomic_t stop_requested = 0;

static int epoll_fd = -1;
static Client *clients = NULL;

static void handle_sigint(int signal_number) {
    (void)signal_number;
    stop_requested = 1;
}

static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);

    if (flags == -1) {
        return -1;
    }

    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static Client *find_client(int fd) {
    for (Client *client = clients;
         client != NULL;
         client = client->next) {
        if (client->fd == fd) {
            return client;
        }
    }

    return NULL;
}

static int update_client_events(Client *client) {
    struct epoll_event event = {0};

    event.events = EPOLLIN | EPOLLRDHUP;

    if (client->queue_count > 0) {
        event.events |= EPOLLOUT;
    }

    event.data.fd = client->fd;

    return epoll_ctl(epoll_fd, EPOLL_CTL_MOD,
                     client->fd, &event);
}

static int queue_message(Client *client, const ChatMessage *message) {
    if (client->queue_count == OUTPUT_QUEUE_SIZE) {
        fprintf(stderr,
                "Очередь сообщений клиента %s переполнена.\n",
                client->hostname);
        return -1;
    }

    size_t tail = (client->queue_head + client->queue_count)
                  % OUTPUT_QUEUE_SIZE;

    client->output_queue[tail] = *message;
    client->queue_count++;

    if (update_client_events(client) == -1) {
        perror("epoll_ctl client MOD");
        return -1;
    }

    return 0;
}

static void broadcast_message(const ChatMessage *message, int excluded_fd) {
    for (Client *client = clients;
         client != NULL;
         client = client->next) {

        if (client->fd == excluded_fd || !client->joined) {
            continue;
        }

        (void)queue_message(client, message);
    }
}

static void remove_client(Client *client, int announce) {
    if (announce && client->joined) {
        ChatMessage message = {0};

        message.type = htonl(CHAT_MESSAGE_LEAVE);

        snprintf(message.hostname, sizeof(message.hostname),
                 "%s", client->hostname);

        broadcast_message(&message, client->fd);
    }

    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client->fd, NULL);
    close(client->fd);

    Client **current = &clients;

    while (*current != NULL) {
        if (*current == client) {
            *current = client->next;
            break;
        }

        current = &(*current)->next;
    }

    printf("Клиент отключён: %s\n",
           client->hostname[0] != '\0'
               ? client->hostname : "(ещё не представился)");

    free(client);
}

static int flush_client(Client *client) {
    while (client->queue_count > 0) {
        ChatMessage *message =
            &client->output_queue[client->queue_head];

        const char *data = (const char *)message;
        size_t remaining = sizeof(*message) - client->sent_bytes;

        ssize_t sent = send(client->fd,
                            data + client->sent_bytes,
                            remaining, MSG_NOSIGNAL);

        if (sent == -1) {
            if (errno == EINTR) {
                continue;
            }

            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }

            return -1;
        }

        if (sent == 0) {
            return -1;
        }

        client->sent_bytes += (size_t)sent;

        if (client->sent_bytes == sizeof(*message)) {
            client->sent_bytes = 0;
            client->queue_head =
                (client->queue_head + 1) % OUTPUT_QUEUE_SIZE;
            client->queue_count--;
        }
    }

    if (update_client_events(client) == -1) {
        return -1;
    }

    return 0;
}

static int process_message(Client *client, const ChatMessage *message) {
    uint32_t type = ntohl(message->type);

    if (type == CHAT_MESSAGE_JOIN) {
        if (client->joined) {
            return 0;
        }

        client->joined = 1;

        snprintf(client->hostname, sizeof(client->hostname),
                 "%s", message->hostname);

        client->hostname[CHAT_HOSTNAME_LENGTH - 1] = '\0';

        printf("Подключился участник: %s\n",
               client->hostname);

        broadcast_message(message, client->fd);
    } else if (type == CHAT_MESSAGE_TEXT ||
            type == CHAT_MESSAGE_FILE_START ||
            type == CHAT_MESSAGE_FILE_DATA ||
            type == CHAT_MESSAGE_FILE_END) {
        if (type == CHAT_MESSAGE_FILE_DATA &&
            ntohl(message->data_length) > sizeof(message->text)) {
            fprintf(stderr, "Получен некорректный размер блока файла.\n");
            return 1;
        }

        if (client->joined) {
            ChatMessage outgoing = *message;

            snprintf(outgoing.hostname, sizeof(outgoing.hostname),
                    "%s", client->hostname);

            broadcast_message(&outgoing, client->fd);
        }
    } else if (type == CHAT_MESSAGE_LEAVE) {
        if (client->joined) {
            ChatMessage outgoing = {0};

            outgoing.type = htonl(CHAT_MESSAGE_LEAVE);

            snprintf(outgoing.hostname, sizeof(outgoing.hostname),
                     "%s", client->hostname);

            broadcast_message(&outgoing, client->fd);

            client->joined = 0;
        }

        return 1;
    }

    return 0;
}

static int read_client(Client *client) {
    for (;;) {
        char *destination = (char *)&client->incoming;
        size_t remaining = sizeof(client->incoming)
                           - client->received_bytes;

        ssize_t received = recv(client->fd,
                                destination + client->received_bytes,
                                remaining, 0);

        if (received == -1) {
            if (errno == EINTR) {
                continue;
            }

            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return 0;
            }

            return -1;
        }

        if (received == 0) {
            return -1;
        }

        client->received_bytes += (size_t)received;

        if (client->received_bytes == sizeof(client->incoming)) {
            uint32_t type = ntohl(client->incoming.type);

            client->incoming.hostname[CHAT_HOSTNAME_LENGTH - 1] = '\0';

            if (type != CHAT_MESSAGE_FILE_DATA)
                client->incoming.text[CHAT_TEXT_LENGTH - 1] = '\0';

            int should_disconnect = process_message(client, &client->incoming);

            client->received_bytes = 0;

            return should_disconnect ? -1 : 0;
        }
    }
}

static int accept_clients(int server_fd) {
    for (;;) {
        int client_fd = accept(server_fd, NULL, NULL);

        if (client_fd == -1) {
            if (errno == EINTR) {
                continue;
            }

            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return 0;
            }

            perror("accept");
            return -1;
        }

        if (set_nonblocking(client_fd) == -1) {
            perror("fcntl client");
            close(client_fd);
            continue;
        }

        Client *client = calloc(1, sizeof(*client));

        if (client == NULL) {
            perror("calloc");
            close(client_fd);
            continue;
        }

        client->fd = client_fd;
        client->next = clients;
        clients = client;

        struct epoll_event event = {0};

        event.events = EPOLLIN | EPOLLRDHUP;
        event.data.fd = client_fd;

        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD,
                      client_fd, &event) == -1) {
            perror("epoll_ctl client ADD");
            remove_client(client, 0);
            continue;
        }

        printf("Новое TCP-подключение, fd=%d\n", client_fd);
    }
}

int run_server(void) {
    int server_fd = -1;
    int result = EXIT_FAILURE;

    struct sigaction action = {0};
    struct sigaction old_action;

    action.sa_handler = handle_sigint;
    sigemptyset(&action.sa_mask);

    if (sigaction(SIGINT, &action, &old_action) == -1) {
        perror("sigaction");
        return EXIT_FAILURE;
    }

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd == -1) {
        perror("socket");
        goto cleanup;
    }

    int option = 1;

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,
                   &option, sizeof(option)) == -1) {
        perror("setsockopt");
        goto cleanup;
    }

    struct sockaddr_in address = {0};

    address.sin_family = AF_INET;
    address.sin_port = htons(CHAT_PORT);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server_fd, (struct sockaddr *)&address,
             sizeof(address)) == -1) {
        perror("bind");
        goto cleanup;
    }

    if (listen(server_fd, 10) == -1) {
        perror("listen");
        goto cleanup;
    }

    if (set_nonblocking(server_fd) == -1) {
        perror("fcntl server");
        goto cleanup;
    }

    epoll_fd = epoll_create1(0);

    if (epoll_fd == -1) {
        perror("epoll_create1");
        goto cleanup;
    }

    struct epoll_event event = {0};

    event.events = EPOLLIN;
    event.data.fd = server_fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD,
                  server_fd, &event) == -1) {
        perror("epoll_ctl server ADD");
        goto cleanup;
    }

    printf("TCP-сервер запущен на порту %d.\n", CHAT_PORT);
    printf("Для остановки нажмите Ctrl+C.\n");

    struct epoll_event events[MAX_EVENTS];

    result = EXIT_SUCCESS;

    while (!stop_requested) {
        int ready = epoll_wait(epoll_fd, events,
                               MAX_EVENTS, 500);

        if (ready == -1) {
            if (errno == EINTR) {
                continue;
            }

            perror("epoll_wait");
            result = EXIT_FAILURE;
            break;
        }

        for (int i = 0; i < ready; i++) {
            int fd = events[i].data.fd;
            uint32_t flags = events[i].events;

            if (fd == server_fd) {
                if (accept_clients(server_fd) == -1) {
                    result = EXIT_FAILURE;
                    stop_requested = 1;
                    break;
                }

                continue;
            }

            Client *client = find_client(fd);

            if (client == NULL) {
                continue;
            }

            if (flags & EPOLLIN) {
                if (read_client(client) == -1) {
                    remove_client(client, 1);
                    continue;
                }
            }

            client = find_client(fd);

            if (client == NULL) {
                continue;
            }

            if (flags & EPOLLOUT) {
                if (flush_client(client) == -1) {
                    remove_client(client, 1);
                    continue;
                }
            }

            if (flags & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                remove_client(client, 1);
            }
        }
    }

cleanup:
    while (clients != NULL) {
        remove_client(clients, 0);
    }

    if (epoll_fd != -1) {
        close(epoll_fd);
    }

    if (server_fd != -1) {
        close(server_fd);
    }

    printf("Сервер остановлен.\n");

    if (sigaction(SIGINT, &old_action, NULL) == -1) {
        perror("sigaction restore");
        result = EXIT_FAILURE;
    }

    return result;
}
