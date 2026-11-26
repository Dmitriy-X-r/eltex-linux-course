#include "protocol.h"
#include "server.h"

#include <errno.h>
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
    uint16_t port = SERVER_DEFAULT_PORT;

    if (argc > 2) {
        fprintf(stderr, "Usage: %s [port]\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (argc == 2 && parse_port(argv[1], &port) != 0) {
        fprintf(stderr, "Invalid port: %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    return run_server(port) == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
