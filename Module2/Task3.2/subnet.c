#include "subnet.h"

#include <stdlib.h>

int parse_ipv4(const char *text, uint32_t *address) {
    if(text == NULL || address == NULL) {
        return 0;
    }

    uint32_t result = 0;
    const char *p = text;

    for (int i = 0; i < 4; i++) {
        if(*p < '0' || *p > '9') {
            return 0;
        }

        unsigned int octet = 0;
        int digits = 0;

        while(*p >= '0' && *p <= '9') {
            octet = octet * 10 + (unsigned int)(*p - '0');
            digits++;

            if(octet > 255 || digits > 3) {
                return 0;
            }

            p++;
        }

        result = (result << 8) | octet;

        if(i < 3) {
            if(*p != '.') {
                return 0;
            }

            p++;
        }
        else if(*p != '\0') {
            return 0;
        }
    }

    *address = result;
    return 1;
}

int is_valid_netmask(uint32_t mask) {
    int zero_found = 0;

    for(int i = 31; i >= 0; i--) {
        unsigned int bit = (mask >> i) & 1U;

        if(bit == 0) {
            zero_found = 1;
        }
        else if (zero_found) {
            return 0;
        }
    }

    return 1;
}

int is_same_subnet(uint32_t gateway_ip, uint32_t mask, uint32_t destination_ip) {
    uint32_t gateway_network = gateway_ip & mask;
    uint32_t destination_network = destination_ip & mask;

    return gateway_network == destination_network;
}

uint32_t generate_random_ipv4() {
    uint32_t octet1 = (uint32_t)(rand() % 256);
    uint32_t octet2 = (uint32_t)(rand() % 256);
    uint32_t octet3 = (uint32_t)(rand() % 256);
    uint32_t octet4 = (uint32_t)(rand() % 256);

    return (octet1 << 24) |
        (octet2 << 16) |
        (octet3 << 8) |
        octet4;
}

PacketStats simulate_packets(uint32_t gateway_ip, uint32_t mask, size_t packet_count) {
    PacketStats stats = {0, 0};

    for(size_t i = 0; i < packet_count; i++) {
        uint32_t destination_ip = generate_random_ipv4();

        if(is_same_subnet(gateway_ip, mask, destination_ip)) {
            stats.local_packets++;
        } else {
            stats.remote_packets++;
        }
    }

    return stats;
}