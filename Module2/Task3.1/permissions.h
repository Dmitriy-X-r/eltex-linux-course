#ifndef PERMISSIONS_H
#define PERMISSIONS_H

#include <sys/types.h>

int parse_permissions(const char *input, mode_t *permissions);
void permissions_to_symbolic(mode_t permissions, char output[10]);
void permissions_to_binary(mode_t permissions, char output[10]);
int get_file_permissions(const char *filename, mode_t *permissions);
int apply_permission_command(const char *command, mode_t *permissions);

#endif