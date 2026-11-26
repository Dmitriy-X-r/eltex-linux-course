
#define _POSIX_C_SOURCE 200809L

#include "server.h"
#include "packet.h"
#include "protocol.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <signal.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct ClientEntry {
    uint32_t ip;
    uint16_t port;
    unsigned long long message_count;
    struct ClientEntry *next;
} ClientEntry;

static ClientEntry *clients[CLIENT_BUCKET_COUNT];
static volatile sig_atomic_t stop_requested = 0;

static void handle_signal(int signal_number){
    (void)signal_number;
    stop_requested = 1;
}

static int install_signal_handlers(void){
    struct sigaction action = {0};

    action.sa_handler = handle_signal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    if (sigaction(SIGINT, &action, NULL) == -1 ||
        sigaction(SIGTERM, &action, NULL) == -1) {
        perror("sigaction");
        return -1;
    }

    return 0;
}

static size_t hash_client(uint32_t ip, uint16_t port){
    uint32_t value = ip ^ ((uint32_t)port << 16) ^ port;

    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    value *= 0x846ca68bU;
    value ^= value >> 16;

    return value % CLIENT_BUCKET_COUNT;
}

static ClientEntry *find_client(struct in_addr ip, uint16_t port){
    size_t bucket = hash_client(ip.s_addr, port);

    for (ClientEntry *entry = clients[bucket];
         entry != NULL;
         entry = entry->next) {
        if (entry->ip == ip.s_addr && entry->port == port) {
            return entry;
        }
    }

    return NULL;
}

static ClientEntry *get_or_create_client(struct in_addr ip, uint16_t port){
    ClientEntry *entry = find_client(ip, port);

    if (entry != NULL) {
        return entry;
    }

    size_t bucket = hash_client(ip.s_addr, port);

    entry = calloc(1, sizeof(*entry));

    if (entry == NULL) {
        perror("calloc");
        return NULL;
    }

    entry->ip = ip.s_addr;
    entry->port = port;
    entry->next = clients[bucket];
    clients[bucket] = entry;

    return entry;
}

static void remove_client(struct in_addr ip, uint16_t port){
    size_t bucket = hash_client(ip.s_addr, port);
    ClientEntry **current = &clients[bucket];

    while (*current != NULL) {
        ClientEntry *entry = *current;

        if (entry->ip == ip.s_addr && entry->port == port) {
            *current = entry->next;
            free(entry);
            printf("Client %s:%u disconnected\n",
                   inet_ntoa(ip), (unsigned int)port);
            return;
        }

        current = &entry->next;
    }
}

static void cleanup_clients(void){
    for (size_t i = 0; i < CLIENT_BUCKET_COUNT; ++i) {
        ClientEntry *entry = clients[i];

        while (entry != NULL) {
            ClientEntry *next = entry->next;
            free(entry);
            entry = next;
        }

        clients[i] = NULL;
    }
}

static int create_raw_sender(void){
    int fd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);

    if (fd == -1) {
        perror("socket sender");
        return -1;
    }

    int enabled = 1;

    if (setsockopt(fd, IPPROTO_IP, IP_HDRINCL,
                   &enabled, sizeof(enabled)) == -1) {
        perror("setsockopt IP_HDRINCL");
        close(fd);
        return -1;
    }

    return fd;
}

static int create_raw_receiver(void){
    int fd = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);

    if (fd == -1) {
        perror("socket receiver");
        return -1;
    }

    return fd;
}

static void process_packet(int send_fd,
                           uint16_t server_port,
                           const unsigned char *packet,
                           size_t packet_length)
{
    struct in_addr source_ip;
    struct in_addr destination_ip;
    uint16_t source_port;
    uint16_t destination_port;
    const unsigned char *payload;
    size_t payload_length;

    if (!parse_udp_packet(packet, packet_length,
                          &source_ip, &source_port,
                          &destination_ip, &destination_port,
                          &payload, &payload_length)) {
        return;
    }

    (void)destination_ip;

    if (destination_port != server_port ||
        payload_length > MAX_MESSAGE_SIZE) {
        return;
    }

    if (payload_length == strlen(CLOSE_MESSAGE) &&
        memcmp(payload, CLOSE_MESSAGE, payload_length) == 0) {
        remove_client(source_ip, source_port);
        return;
    }

    ClientEntry *client = get_or_create_client(source_ip, source_port);

    if (client == NULL) {
        return;
    }

    ++client->message_count;

    char response[MAX_MESSAGE_SIZE + 32];

    int response_length = snprintf(
        response, sizeof(response), "%.*s %llu",
        (int)payload_length, (const char *)payload,
        client->message_count);

    if (response_length < 0 ||
        (size_t)response_length >= sizeof(response)) {
        return;
    }

    struct in_addr any_source = { .s_addr = INADDR_ANY };

    if (send_udp_packet(send_fd,
                        any_source, server_port,
                        source_ip, source_port,
                        response, (size_t)response_length) == -1) {
        perror("send_udp_packet");
        return;
    }

    printf("%s:%u -> %.*s (message %llu)\n",
           inet_ntoa(source_ip),
           (unsigned int)source_port,
           (int)payload_length,
           (const char *)payload,
           client->message_count);
}

int run_server(uint16_t server_port){
    if (install_signal_handlers() == -1) {
        return -1;
    }

    int receive_fd = create_raw_receiver();

    if (receive_fd == -1) {
        return -1;
    }

    int send_fd = create_raw_sender();

    if (send_fd == -1) {
        close(receive_fd);
        return -1;
    }

    printf("Raw UDP echo server listening on port %u\n",
           (unsigned int)server_port);

    struct pollfd descriptor = {
        .fd = receive_fd,
        .events = POLLIN
    };

    unsigned char packet[65536];

    while (!stop_requested) {
        int ready = poll(&descriptor, 1, -1);

        if (ready == -1) {
            if (errno == EINTR) {
                continue;
            }

            perror("poll");
            break;
        }

        if (descriptor.revents & POLLIN) {
            ssize_t received = recv(receive_fd,
                                    packet, sizeof(packet), 0);

            if (received == -1) {
                if (errno == EINTR) {
                    continue;
                }

                perror("recv");
                break;
            }

            process_packet(send_fd, server_port,
                           packet, (size_t)received);
        }
    }

    close(send_fd);
    close(receive_fd);
    cleanup_clients();

    printf("Server stopped\n");
    return 0;
}
