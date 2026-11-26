#include "protocol.h"
#include "packet.h"

#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <sys/socket.h>
#include <string.h>

static uint16_t calculate_ip_checksum(const unsigned char *data, size_t length){
    uint32_t sum = 0;

    for (size_t i = 0; i + 1 < length; i += 2) {
        sum += ((uint16_t)data[i] << 8) | data[i + 1];
    }

    if (length % 2 != 0) {
        sum += (uint16_t)data[length - 1] << 8;
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFFU) + (sum >> 16);
    }

    return (uint16_t)~sum;
}

int send_udp_packet(int socket_fd,
                    struct in_addr source_ip,
                    uint16_t source_port,
                    struct in_addr destination_ip,
                    uint16_t destination_port,
                    const void *payload,
                    size_t payload_length)
{
    unsigned char packet[sizeof(struct iphdr) +
                         sizeof(struct udphdr) +
                         MAX_MESSAGE_SIZE];

    if (payload_length > MAX_MESSAGE_SIZE) {
        return -1;
    }

    size_t ip_length = sizeof(struct iphdr);
    size_t udp_length = sizeof(struct udphdr) + payload_length;
    size_t packet_length = ip_length + udp_length;

    struct iphdr ip_header = {0};
    struct udphdr udp_header = {0};

    ip_header.version = 4;
    ip_header.ihl = 5;
    ip_header.tos = 0;
    ip_header.tot_len = htons((uint16_t)packet_length);
    ip_header.id = 0;
    ip_header.frag_off = 0;
    ip_header.ttl = 64;
    ip_header.protocol = IPPROTO_UDP;
    ip_header.saddr = source_ip.s_addr;
    ip_header.daddr = destination_ip.s_addr;
    ip_header.check = 0;

    ip_header.check = htons(
        calculate_ip_checksum((const unsigned char *)&ip_header,
                              sizeof(ip_header)));

    udp_header.source = htons(source_port);
    udp_header.dest = htons(destination_port);
    udp_header.len = htons((uint16_t)udp_length);
    udp_header.check = 0;

    memcpy(packet, &ip_header, sizeof(ip_header));
    memcpy(packet + ip_length, &udp_header, sizeof(udp_header));

    if (payload_length > 0) {
        memcpy(packet + ip_length + sizeof(udp_header),
               payload,
               payload_length);
    }

    struct sockaddr_in destination = {0};
    destination.sin_family = AF_INET;
    destination.sin_addr = destination_ip;
    destination.sin_port = htons(destination_port);

    ssize_t sent = sendto(socket_fd,
                          packet,
                          packet_length,
                          0,
                          (struct sockaddr *)&destination,
                          sizeof(destination));

    if (sent < 0 || (size_t)sent != packet_length) {
        return -1;
    }

    return 0;
}

int parse_udp_packet(const unsigned char *packet,
                     size_t packet_length,
                     struct in_addr *source_ip,
                     uint16_t *source_port,
                     struct in_addr *destination_ip,
                     uint16_t *destination_port,
                     const unsigned char **payload,
                     size_t *payload_length)
{
    if (packet_length < sizeof(struct iphdr)) {
        return 0;
    }

    struct iphdr ip_header;
    memcpy(&ip_header, packet, sizeof(ip_header));

    if (ip_header.version != 4 || ip_header.ihl < 5) {
        return 0;
    }

    if (ip_header.protocol != IPPROTO_UDP) {
        return 0;
    }

    size_t ip_header_length = (size_t)ip_header.ihl * 4;
    size_t total_length = ntohs(ip_header.tot_len);

    if (ip_header_length > packet_length ||
        total_length > packet_length ||
        total_length < ip_header_length + sizeof(struct udphdr)) {
        return 0;
    }

    struct udphdr udp_header;
    memcpy(&udp_header, packet + ip_header_length, sizeof(udp_header));

    size_t udp_length = ntohs(udp_header.len);

    if (udp_length < sizeof(struct udphdr) ||
        udp_length > total_length - ip_header_length) {
        return 0;
    }

    *source_ip = (struct in_addr){ .s_addr = ip_header.saddr };
    *destination_ip = (struct in_addr){ .s_addr = ip_header.daddr };
    *source_port = ntohs(udp_header.source);
    *destination_port = ntohs(udp_header.dest);

    *payload = packet + ip_header_length + sizeof(struct udphdr);
    *payload_length = udp_length - sizeof(struct udphdr);

    return 1;
}
