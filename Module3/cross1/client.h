#ifndef CLIENT_H
#define CLIENT_H

#include <stdint.h>

int run_client(const char *server_address,
               uint16_t server_port,
               uint16_t client_port);

#endif
