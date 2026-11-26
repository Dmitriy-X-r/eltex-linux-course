#define _POSIX_C_SOURCE 200809L

#include "client.h"
#include "packet.h"
#include "protocol.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

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


static void receive_responses(int receive_fd,
                              struct in_addr server_ip,
                              uint16_t server_port,
                              uint16_t client_port)
{
    unsigned char packet[65536];

    ssize_t received = recv(receive_fd, packet, sizeof(packet), 0);

    if (received == -1) {
        if (errno != EINTR) {
            perror("recv");
        }
        return;
    }

    struct in_addr source_ip;
    struct in_addr destination_ip;
    uint16_t source_port;
    uint16_t destination_port;
    const unsigned char *payload;
    size_t payload_length;

    if (!parse_udp_packet(packet, (size_t)received,
                          &source_ip, &source_port,
                          &destination_ip, &destination_port,
                          &payload, &payload_length)) {
        return;
    }

    (void)destination_ip;

    if (source_ip.s_addr != server_ip.s_addr ||
        source_port != server_port ||
        destination_port != client_port) {
        return;
    }

    printf("Server: %.*s\n",
           (int)payload_length, (const char *)payload);
    fflush(stdout);
}


int run_client(const char *server_address,
               uint16_t server_port,
               uint16_t client_port)
{
    struct in_addr server_ip;

    if (inet_pton(AF_INET, server_address, &server_ip) != 1) {
        fprintf(stderr, "Invalid IPv4 address: %s\n", server_address);
        return -1;
    }

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

    printf("Connected logically to %s:%u, client port %u\n",
           server_address,
           (unsigned int)server_port,
           (unsigned int)client_port);
    printf("Enter a message. Press Ctrl+C to exit.\n");

    struct pollfd descriptors[2] = {
        { .fd = STDIN_FILENO, .events = POLLIN },
        { .fd = receive_fd, .events = POLLIN }
    };

    char message[MAX_MESSAGE_SIZE + 1];

    while (!stop_requested) {
        int ready = poll(descriptors, 2, -1);

        if (ready == -1) {
            if (errno == EINTR) {
                continue;
            }

            perror("poll");
            break;
        }

        if (descriptors[1].revents & POLLIN) {
            receive_responses(receive_fd, server_ip,
                              server_port, client_port);
        }

        if (stop_requested) {
            break;
        }

        if (descriptors[0].revents & (POLLIN | POLLHUP)) {
            if (fgets(message, sizeof(message), stdin) == NULL) {
                if (ferror(stdin) && errno == EINTR) {
                    clearerr(stdin);
                    continue;
                }

                break;
            }

            size_t length = strcspn(message, "\n");

            if (message[length] == '\n') {
                message[length] = '\0';
            } else if (!feof(stdin)) {
                int ch;

                while ((ch = getchar()) != '\n' && ch != EOF) {}
            }

            if (length == 0) {
                continue;
            }

            if (send_udp_packet(send_fd,
                                (struct in_addr){ .s_addr = INADDR_ANY },
                                client_port,
                                server_ip,
                                server_port,
                                message, length) == -1) {
                perror("send_udp_packet");
            }
        }
    }

    if (send_udp_packet(send_fd,
                        (struct in_addr){ .s_addr = INADDR_ANY },
                        client_port,
                        server_ip,
                        server_port,
                        CLOSE_MESSAGE, strlen(CLOSE_MESSAGE)) == -1) {
        perror("send close message");
    } else {
        printf("Close message sent\n");
    }

    close(send_fd);
    close(receive_fd);

    return 0;
}
