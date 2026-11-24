#define _POSIX_C_SOURCE 200809L

#include "capture.h"
#include "chat.h"

#include <arpa/inet.h>
#include <errno.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define CHAT_FILTER 1
#define DNS_FILTER  2
#define PACKET_BUFFER_SIZE 65536
#define CHAT_PORT 45454
#define DNS_PORT 53

static volatile sig_atomic_t stop_requested = 0;

static void handle_sigint(int signal_number)
{
    (void)signal_number;
    stop_requested = 1;
}

static double elapsed_seconds(const struct timespec *start)
{
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) == -1) {
        return 0.0;
    }

    return (double)(now.tv_sec - start->tv_sec) +
           (double)(now.tv_nsec - start->tv_nsec) / 1000000000.0;
}

static void output(FILE *log, const char *format, ...)
{
    va_list args;
    va_list file_args;

    va_start(args, format);
    va_copy(file_args, args);

    vprintf(format, args);
    vfprintf(log, format, file_args);

    va_end(file_args);
    va_end(args);

    fflush(stdout);
    fflush(log);
}

static void print_mac(FILE *log, const unsigned char *mac)
{
    output(log, "%02x:%02x:%02x:%02x:%02x:%02x",
           mac[0], mac[1], mac[2],
           mac[3], mac[4], mac[5]);
}

static int is_dns_packet(uint16_t source_port,
                         uint16_t destination_port,
                         const unsigned char *payload,
                         size_t payload_length)
{
    if (source_port != DNS_PORT &&
        destination_port != DNS_PORT) {
        return 0;
    }

    /* Минимальный размер DNS-заголовка — 12 байт. */
    if (payload_length < 12) {
        return 0;
    }

    /*
     * Проверяем, что флаги и счётчики соответствуют
     * допустимому DNS-сообщению.
     */
    uint16_t flags =
        ((uint16_t)payload[2] << 8) | payload[3];

    uint16_t questions =
        ((uint16_t)payload[4] << 8) | payload[5];

    uint16_t answers =
        ((uint16_t)payload[6] << 8) | payload[7];

    uint16_t authority =
        ((uint16_t)payload[8] << 8) | payload[9];

    uint16_t additional =
        ((uint16_t)payload[10] << 8) | payload[11];

    /* Зарезервированные биты DNS-флагов должны быть нулевыми. */
    if ((flags & 0x0070U) != 0) {
        return 0;
    }

    /*
     * У DNS-запроса обычно есть хотя бы один вопрос.
     * Ответ может содержать вопросы, ответы и дополнительные
     * записи, но пустой пакет без записей не принимаем.
     */
    if (questions == 0 &&
        answers == 0 &&
        authority == 0 &&
        additional == 0) {
        return 0;
    }

    return 1;
}

static int is_chat_packet(uint16_t source_port,
                          uint16_t destination_port,
                          const unsigned char *payload,
                          size_t payload_length)
{
    if (source_port != CHAT_PORT &&
        destination_port != CHAT_PORT) {
        return 0;
    }

    if (payload_length != sizeof(ChatMessage)) {
        return 0;
    }

    ChatMessage message;
    memcpy(&message, payload, sizeof(message));

    if (message.type < CHAT_MESSAGE_JOIN ||
        message.type > CHAT_MESSAGE_LEAVE) {
        return 0;
    }

    return 1;
}

static void print_chat_details(FILE *log,
                               const unsigned char *payload)
{
    ChatMessage message;
    memcpy(&message, payload, sizeof(message));

    message.hostname[CHAT_HOSTNAME_LENGTH - 1] = '\0';
    message.text[CHAT_TEXT_LENGTH - 1] = '\0';

    const char *type_name;

    switch (message.type) {
    case CHAT_MESSAGE_JOIN:
        type_name = "подключение";
        break;

    case CHAT_MESSAGE_TEXT:
        type_name = "сообщение";
        break;

    case CHAT_MESSAGE_LEAVE:
        type_name = "отключение";
        break;

    default:
        type_name = "неизвестный тип";
        break;
    }

    output(log, "Чат: %s, участник: %s\n",
           type_name, message.hostname);

    if (message.type == CHAT_MESSAGE_TEXT) {
        output(log, "Текст: %s\n", message.text);
    }
}

static void print_dns_details(FILE *log,
                              const unsigned char *payload)
{
    uint16_t id =
        ((uint16_t)payload[0] << 8) | payload[1];

    uint16_t flags =
        ((uint16_t)payload[2] << 8) | payload[3];

    uint16_t questions =
        ((uint16_t)payload[4] << 8) | payload[5];

    uint16_t answers =
        ((uint16_t)payload[6] << 8) | payload[7];

    output(log, "DNS ID: 0x%04x, тип: %s\n",
           id,
           (flags & 0x8000U) ? "ответ" : "запрос");

    output(log, "DNS: вопросов %u, ответов %u\n",
           (unsigned int)questions,
           (unsigned int)answers);
}

static int process_packet(const unsigned char *buffer,
                          size_t packet_length,
                          int filter,
                          double elapsed,
                          FILE *log)
{
    /*
     * Минимальный Ethernet-заголовок.
     * Если кадр слишком короткий, читать его нельзя.
     */
    if (packet_length < sizeof(struct ether_header)) {
        return 0;
    }

    struct ether_header ethernet;
    memcpy(&ethernet, buffer, sizeof(ethernet));

    /* Пока обрабатываем только IPv4. */
    if (ntohs(ethernet.ether_type) != ETHERTYPE_IP) {
        return 0;
    }

    size_t ip_offset = sizeof(struct ether_header);

    if (packet_length < ip_offset + sizeof(struct iphdr)) {
        return 0;
    }

    struct iphdr ip;
    memcpy(&ip, buffer + ip_offset, sizeof(ip));

    if (ip.version != 4 ||
        ip.ihl < 5 ||
        ip.protocol != IPPROTO_UDP) {
        return 0;
    }

    size_t ip_header_length = (size_t)ip.ihl * 4;
    size_t ip_total_length = ntohs(ip.tot_len);

    if (ip_total_length < ip_header_length + sizeof(struct udphdr) ||
        ip_total_length > packet_length - ip_offset) {
        return 0;
    }

    /*
     * Фрагмент IPv4 с ненулевым смещением не содержит
     * начала UDP-заголовка. Его нельзя разбирать как
     * самостоятельный UDP-пакет.
     */
    uint16_t fragment_field = ntohs(ip.frag_off);

    if ((fragment_field & 0x1FFFU) != 0) {
        return 0;
    }

    size_t udp_offset = ip_offset + ip_header_length;

    struct udphdr udp;
    memcpy(&udp, buffer + udp_offset, sizeof(udp));

    uint16_t source_port = ntohs(udp.source);
    uint16_t destination_port = ntohs(udp.dest);
    size_t udp_length = ntohs(udp.len);

    if (udp_length < sizeof(struct udphdr) ||
        udp_length > ip_total_length - ip_header_length) {
        return 0;
    }

    size_t payload_length = udp_length - sizeof(struct udphdr);
    const unsigned char *payload =
        buffer + udp_offset + sizeof(struct udphdr);

    int matched;

    if (filter == CHAT_FILTER) {
        matched = is_chat_packet(source_port,
                                 destination_port,
                                 payload,
                                 payload_length);
    } else if (filter == DNS_FILTER) {
        matched = is_dns_packet(source_port,
                                destination_port,
                                payload,
                                payload_length);
    } else {
        return 0;
    }

    if (!matched) {
        return 0;
    }

    char source_ip[INET_ADDRSTRLEN];
    char destination_ip[INET_ADDRSTRLEN];

    if (inet_ntop(AF_INET, &ip.saddr,
                  source_ip, sizeof(source_ip)) == NULL ||
        inet_ntop(AF_INET, &ip.daddr,
                  destination_ip, sizeof(destination_ip)) == NULL) {
        return 0;
    }

    output(log, "\n========================================\n");
    output(log, "Время с начала захвата: %.3f сек\n", elapsed);

    output(log, "MAC отправителя: ");
    print_mac(log, ethernet.ether_shost);
    output(log, "\n");

    output(log, "MAC получателя:  ");
    print_mac(log, ethernet.ether_dhost);
    output(log, "\n");

    output(log, "IP отправителя:  %s\n", source_ip);
    output(log, "IP получателя:   %s\n", destination_ip);

    output(log, "UDP-порт отправителя: %u\n",
           (unsigned int)source_port);

    output(log, "UDP-порт получателя:  %u\n",
           (unsigned int)destination_port);

    output(log, "Размер UDP-данных: %zu байт\n",
           payload_length);

    if (filter == CHAT_FILTER) {
        print_chat_details(log, payload);
    } else {
        print_dns_details(log, payload);
    }

    return 1;
}

int run_capture(int filter, int duration_seconds)
{
    if ((filter != CHAT_FILTER && filter != DNS_FILTER) ||
        duration_seconds < 1 || duration_seconds > 3600) {
        fprintf(stderr, "Некорректные параметры захвата.\n");
        return EXIT_FAILURE;
    }

    FILE *log = fopen("capture.txt", "w");

    if (log == NULL) {
        perror("fopen capture.txt");
        return EXIT_FAILURE;
    }

    int socket_fd = socket(AF_PACKET,
                           SOCK_RAW,
                           htons(ETH_P_ALL));

    if (socket_fd == -1) {
        perror("socket AF_PACKET");
        fclose(log);
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
        fclose(log);
        return EXIT_FAILURE;
    }

    stop_requested = 0;

    const char *filter_name =
        (filter == CHAT_FILTER) ? "UDP-чат" : "DNS";

    output(log, "Начало захвата. Фильтр: %s\n", filter_name);
    output(log, "Длительность: %d сек.\n", duration_seconds);
    output(log, "Для досрочного завершения нажмите Ctrl+C.\n");

    struct timespec start;

    if (clock_gettime(CLOCK_MONOTONIC, &start) == -1) {
        perror("clock_gettime");
        sigaction(SIGINT, &old_action, NULL);
        close(socket_fd);
        fclose(log);
        return EXIT_FAILURE;
    }

    unsigned char buffer[PACKET_BUFFER_SIZE];

    struct pollfd descriptor = {
        .fd = socket_fd,
        .events = POLLIN
    };

    int capture_error = 0;

    while (!stop_requested) {
        double elapsed = elapsed_seconds(&start);

        if (elapsed >= duration_seconds) {
            break;
        }

        int ready = poll(&descriptor, 1, 250);

        if (ready == -1) {
            if (errno == EINTR) {
                continue;
            }

            perror("poll");
            capture_error = 1;
            break;
        }

        if (ready == 0) {
            continue;
        }

        if (descriptor.revents & (POLLERR | POLLNVAL)) {
            fprintf(stderr, "Ошибка RAW-сокета.\n");
            capture_error = 1;
            break;
        }

        if (!(descriptor.revents & POLLIN)) {
            continue;
        }

        ssize_t received = recvfrom(socket_fd,
                                    buffer,
                                    sizeof(buffer),
                                    0,
                                    NULL,
                                    NULL);

        if (received == -1) {
            if (errno == EINTR) {
                continue;
            }

            perror("recvfrom");
            capture_error = 1;
            break;
        }

        elapsed = elapsed_seconds(&start);

        process_packet(buffer,
                       (size_t)received,
                       filter,
                       elapsed,
                       log);
    }

    output(log, "\nЗахват завершён.\n");
    output(log, "Результаты сохранены в capture.txt\n");

    if (sigaction(SIGINT, &old_action, NULL) == -1) {
        perror("sigaction restore");
        capture_error = 1;
    }

    if (close(socket_fd) == -1) {
        perror("close");
        capture_error = 1;
    }

    if (fclose(log) == EOF) {
        perror("fclose");
        capture_error = 1;
    }

    return capture_error ? EXIT_FAILURE : EXIT_SUCCESS;
}