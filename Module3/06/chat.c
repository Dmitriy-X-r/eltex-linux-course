#define _DEFAULT_SOURCE
#include "chat.h"

#include <arpa/inet.h>
#include <errno.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define INPUT_LENGTH CHAT_TEXT_LENGTH

static volatile sig_atomic_t stop_requested = 0;

static void handle_sigint(int signal_number)
{
    (void)signal_number;
    stop_requested = 1;
}

static int find_broadcast_address(struct sockaddr_in *broadcast_address,
                                  struct in_addr *local_address)
{
    struct ifaddrs *interfaces = NULL;

    if (getifaddrs(&interfaces) == -1) {
        perror("getifaddrs");
        return -1;
    }

    int found = 0;

    for (struct ifaddrs *interface = interfaces;
         interface != NULL;
         interface = interface->ifa_next) {

        if (interface->ifa_addr == NULL ||
            interface->ifa_netmask == NULL) {
            continue;
        }

        if (interface->ifa_addr->sa_family != AF_INET ||
            !(interface->ifa_flags & IFF_UP) ||
            !(interface->ifa_flags & IFF_BROADCAST) ||
            (interface->ifa_flags & IFF_LOOPBACK)) {
            continue;
        }

        struct sockaddr_in *address =
            (struct sockaddr_in *)interface->ifa_addr;

        struct sockaddr_in *netmask =
            (struct sockaddr_in *)interface->ifa_netmask;

        uint32_t ip = ntohl(address->sin_addr.s_addr);
        uint32_t mask = ntohl(netmask->sin_addr.s_addr);
        uint32_t broadcast = (ip & mask) | ~mask;

        memset(broadcast_address, 0, sizeof(*broadcast_address));
        broadcast_address->sin_family = AF_INET;
        broadcast_address->sin_port = htons(CHAT_PORT);
        broadcast_address->sin_addr.s_addr = htonl(broadcast);

        *local_address = address->sin_addr;
        found = 1;
        break;
    }

    freeifaddrs(interfaces);

    if (!found) {
        fprintf(stderr,
                "Не найден подходящий IPv4-интерфейс с broadcast.\n");
        return -1;
    }

    return 0;
}

static int send_chat_message(int socket_fd,
                             const struct sockaddr_in *destination,
                             const ChatMessage *message)
{
    ssize_t sent = sendto(socket_fd,
                          message,
                          sizeof(*message),
                          0,
                          (const struct sockaddr *)destination,
                          sizeof(*destination));

    if (sent == -1) {
        perror("sendto");
        return -1;
    }

    if ((size_t)sent != sizeof(*message)) {
        fprintf(stderr, "Отправлено не всё сообщение.\n");
        return -1;
    }

    return 0;
}

static void receive_chat_message(int socket_fd, uint32_t own_client_id)
{
    ChatMessage message;

    ssize_t received = recvfrom(socket_fd,
                                &message,
                                sizeof(message),
                                0,
                                NULL,
                                NULL);

    if (received == -1) {
        if (errno != EINTR) {
            perror("recvfrom");
        }
        return;
    }

    if ((size_t)received != sizeof(message)) {
        fprintf(stderr, "Получен пакет неверного размера.\n");
        return;
    }

    message.hostname[CHAT_HOSTNAME_LENGTH - 1] = '\0';
    message.text[CHAT_TEXT_LENGTH - 1] = '\0';

    if (message.type < CHAT_MESSAGE_JOIN ||
        message.type > CHAT_MESSAGE_LEAVE) {
        return;
    }

    if (message.client_id == own_client_id) {
        return;
    }

    switch (message.type) {
    case CHAT_MESSAGE_JOIN:
        printf("\n[Сеть] Подключился участник: %s\n",
               message.hostname);
        break;

    case CHAT_MESSAGE_TEXT:
        printf("\n[%s] %s\n", message.hostname, message.text);
        break;

    case CHAT_MESSAGE_LEAVE:
        printf("\n[Сеть] Участник отключился: %s\n",
               message.hostname);
        break;

    default:
        return;
    }

    printf("> ");
    fflush(stdout);
}

int run_chat(void)
{
    struct sockaddr_in broadcast_address;
    struct in_addr local_address;

    if (find_broadcast_address(&broadcast_address, &local_address) == -1) {
        return EXIT_FAILURE;
    }

    char local_ip_text[INET_ADDRSTRLEN];

    if (inet_ntop(AF_INET,
                  &local_address,
                  local_ip_text,
                  sizeof(local_ip_text)) == NULL) {
        perror("inet_ntop");
        return EXIT_FAILURE;
    }

    char hostname[CHAT_HOSTNAME_LENGTH];

    if (gethostname(hostname, sizeof(hostname)) == -1) {
        perror("gethostname");
        return EXIT_FAILURE;
    }

    hostname[CHAT_HOSTNAME_LENGTH - 1] = '\0';

    uint32_t own_client_id =
        (uint32_t)time(NULL) ^ (uint32_t)getpid();

    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (socket_fd == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    int option = 1;

    if (setsockopt(socket_fd,
                   SOL_SOCKET,
                   SO_BROADCAST,
                   &option,
                   sizeof(option)) == -1) {
        perror("setsockopt SO_BROADCAST");
        close(socket_fd);
        return EXIT_FAILURE;
    }

    if (setsockopt(socket_fd,
                   SOL_SOCKET,
                   SO_REUSEADDR,
                   &option,
                   sizeof(option)) == -1) {
        perror("setsockopt SO_REUSEADDR");
        close(socket_fd);
        return EXIT_FAILURE;
    }

    struct sockaddr_in local_socket_address;
    memset(&local_socket_address, 0, sizeof(local_socket_address));

    local_socket_address.sin_family = AF_INET;
    local_socket_address.sin_port = htons(CHAT_PORT);
    local_socket_address.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(socket_fd,
             (const struct sockaddr *)&local_socket_address,
             sizeof(local_socket_address)) == -1) {
        perror("bind");
        close(socket_fd);
        return EXIT_FAILURE;
    }

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_sigint;
    sigemptyset(&action.sa_mask);

    struct sigaction old_action;

    if (sigaction(SIGINT, &action, &old_action) == -1) {
        perror("sigaction");
        close(socket_fd);
        return EXIT_FAILURE;
    }

    /*
     * Отключаем буферизацию stdin, чтобы fgets() не забирал
     * строки, которые poll() ещё не видит на файловом дескрипторе.
     */
    setvbuf(stdin, NULL, _IONBF, 0);

    printf("Компьютер: %s\n", hostname);
    printf("Локальный IPv4: %s\n", local_ip_text);

    char broadcast_ip_text[INET_ADDRSTRLEN];

    if (inet_ntop(AF_INET,
                  &broadcast_address.sin_addr,
                  broadcast_ip_text,
                  sizeof(broadcast_ip_text)) != NULL) {
        printf("Broadcast: %s:%d\n",
               broadcast_ip_text,
               CHAT_PORT);
    }

    printf("Введите сообщение или /quit для выхода.\n");

    ChatMessage message;
    memset(&message, 0, sizeof(message));

    message.type = CHAT_MESSAGE_JOIN;
    message.client_id = own_client_id;
    snprintf(message.hostname, sizeof(message.hostname),
             "%s", hostname);

    if (send_chat_message(socket_fd, &broadcast_address, &message) == -1) {
        sigaction(SIGINT, &old_action, NULL);
        close(socket_fd);
        return EXIT_FAILURE;
    }

    int running = 1;

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

    while (running && !stop_requested) {
        int ready = poll(descriptors, 2, 500);

        if (ready == -1) {
            if (errno == EINTR) {
                continue;
            }

            perror("poll");
            running = 0;
            break;
        }

        if (stop_requested) {
            break;
        }

        if (ready == 0) {
            continue;
        }

        if (descriptors[0].revents & POLLIN) {
            receive_chat_message(socket_fd, own_client_id);
        }

        if (descriptors[0].revents & (POLLERR | POLLNVAL)) {
            fprintf(stderr, "Ошибка UDP-сокета.\n");
            running = 0;
            break;
        }

        if (descriptors[1].revents & POLLIN) {
            char input[INPUT_LENGTH];

            printf("> ");
            fflush(stdout);

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
                continue;
            }

            memset(&message, 0, sizeof(message));
            message.type = CHAT_MESSAGE_TEXT;
            message.client_id = own_client_id;
            snprintf(message.hostname, sizeof(message.hostname),
                     "%s", hostname);
            snprintf(message.text, sizeof(message.text),
                     "%s", input);

            if (send_chat_message(socket_fd,
                                  &broadcast_address,
                                  &message) == -1) {
                running = 0;
                break;
            }
        }

        if (descriptors[1].revents & (POLLERR | POLLNVAL)) {
            fprintf(stderr, "Ошибка стандартного ввода.\n");
            running = 0;
            break;
        }

        if (descriptors[1].revents & POLLHUP) {
            running = 0;
            break;
        }
    }

    memset(&message, 0, sizeof(message));
    message.type = CHAT_MESSAGE_LEAVE;
    message.client_id = own_client_id;
    snprintf(message.hostname, sizeof(message.hostname),
             "%s", hostname);

    if (send_chat_message(socket_fd, &broadcast_address, &message) == -1) {
        fprintf(stderr, "Не удалось отправить уведомление об отключении.\n");
    }

    printf("\nЗавершение работы чата.\n");

    if (sigaction(SIGINT, &old_action, NULL) == -1) {
        perror("sigaction restore");
    }

    if (close(socket_fd) == -1) {
        perror("close");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
