#ifndef PACKET_H
#define PACKET_H

#include <stddef.h>
#include <stdint.h>
#include <netinet/in.h>

int send_udp_packet(int socket_fd,
                    struct in_addr source_ip,
                    uint16_t source_port,
                    struct in_addr destination_ip,
                    uint16_t destination_port,
                    const void *payload,
                    size_t payload_length);

int parse_udp_packet(const unsigned char *packet,
                     size_t packet_length,
                     struct in_addr *source_ip,
                     uint16_t *source_port,
                     struct in_addr *destination_ip,
                     uint16_t *destination_port,
                     const unsigned char **payload,
                     size_t *payload_length);

#endif
