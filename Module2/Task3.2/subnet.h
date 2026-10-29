#ifndef SUBNET_H
#define SUBNET_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    size_t local_packets;
    size_t remote_packets;
} PacketStats;

int parse_ipv4(const char *text, uint32_t *address);
int is_valid_netmask(uint32_t mask);
int is_same_subnet(uint32_t gateway_ip, uint32_t mask, uint32_t destination_ip);
uint32_t generate_random_ipv4();

PacketStats simulate_packets(uint32_t gateway_ip, uint32_t mask, size_t packet_count);

#endif
