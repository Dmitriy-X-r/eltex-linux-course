#include "client.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static int parse_port(const char *text, uint16_t *port){
    char *end = NULL;
    errno = 0;

    unsigned long value = strtoul(text, &end, 10);

    if (errno != 0 || end == text || *end != '\0' ||
        value == 0 || value > 65535) {
        return -1;
    }

    *port = (uint16_t)value;
    return 0;
}

int main(int argc, char *argv[]){
    uint16_t server_port;
    uint16_t client_port;

    if (argc != 4) {
        fprintf(stderr,
                "Usage: %s <server_ipv4> <server_port> <client_port>\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    if (parse_port(argv[2], &server_port) != 0 ||
        parse_port(argv[3], &client_port) != 0) {
        fprintf(stderr, "Invalid port\n");
        return EXIT_FAILURE;
    }

    return run_client(argv[1], server_port, client_port) == 0
               ? EXIT_SUCCESS
               : EXIT_FAILURE;
}
