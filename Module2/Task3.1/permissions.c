#include "permissions.h"

#include <ctype.h>
#include <string.h>
#include <sys/stat.h>

int parse_permissions(const char *input, mode_t *permissions) {
    mode_t result = 0;

    if (input == NULL || permissions == NULL) {
        return -1;
    }

    if (strlen(input) == 3) {
        for (int i = 0; i < 3; i++) {
            if (input[i] < '0' || input[i] > '7') {
                return -1;
            }

            result = result * 8 + (input[i] - '0');
        }

        *permissions = result;
        return 0;
    }

    if (strlen(input) == 9) {
        for (int i = 0; i < 9; i++) {
            char expected;

            switch (i % 3) {
                case 0:
                    expected = 'r';
                    break;
                case 1:
                    expected = 'w';
                    break;
                default:
                    expected = 'x';
                    break;
            }

            if (input[i] == expected) {
                result |= (mode_t)(1 << (8 - i));
            } 
            else if (input[i] != '-') {
                return -1;
            }
        }

        *permissions = result;
        return 0;
    }

    return -1;
}

void permissions_to_symbolic(mode_t permissions, char output[10]) {
    const char symbols[] = "rwx";

    for (int i = 0; i < 9; i++) {
        mode_t bit = (mode_t)(1 << (8 - i));

        if(permissions & bit) {
            output[i] = symbols[i % 3];
        } else {
            output[i] = '-';
        }
    }

    output[9] = '\0';
}

void permissions_to_binary(mode_t permissions, char output[10]) {
    for (int i = 0; i < 9; i++) {
        mode_t bit = (mode_t)(1 << (8 - i));
        output[i] = (permissions & bit) ? '1' : '0';
    }

    output[9] = '\0';
}

int get_file_permissions(const char *filename, mode_t *permissions) {
    struct stat file_info;

    if(filename == NULL || permissions == NULL) {
        return -1;
    }

    if(stat(filename, &file_info) == -1) {
        return -1;
    }

    *permissions = file_info.st_mode & 0777;
    return 0;
}

int apply_permission_command(const char *command, mode_t *permissions) {
    if(command == NULL || permissions == NULL) {
        return -1;
    }

    size_t length = strlen(command);
    size_t operator_index = 0;
    int who_mask = 0;
    mode_t permissions_mask = 0;

    while (operator_index < length &&
            command[operator_index] != '+' &&
            command[operator_index] != '-' &&
            command[operator_index] != '=') {
        operator_index++;
    }

    if (operator_index == 0 || operator_index >= length ||
        operator_index + 1 >= length) {
        return -1;
    }

    for(size_t i = 0; i < operator_index; i++) {
        switch(command[i]) {
            case 'u':
                who_mask |= 4;
                break;
            case 'g':
                who_mask |= 2;
                break;
            case 'o':
                who_mask |= 1;
                break;
            case 'a':
                who_mask |= 7;
                break;
            default:
                return -1;
        }
    }

    for(size_t i = operator_index + 1; i < length; i++) {
        mode_t bit;

        switch (command[i]) {
            case 'r':
                bit = 4;
                break;
            case 'w':
                bit = 2;
                break;
            case 'x':
                bit = 1;
                break;
            default:
                return -1;
        }

        permissions_mask |= bit;
    }

    mode_t result = *permissions;

    for(int group = 0; group < 3; group++) {
        int selected = 1 << (2 - group);

        if ((who_mask & selected) == 0) {
            continue;
        }

        int shift = (2 - group) * 3;
        mode_t mask = (mode_t)(7 << shift);
        mode_t bits = (mode_t)(permissions_mask << shift);

        switch (command[operator_index]) {
            case '+':
                result |= bits;
                break;
            case '-':
                result &= (mode_t)~bits;
                break;
            case '=':
                result = (result & (mode_t)~mask) | bits;
                break;
            default:
                return -1;
        }
    }

    *permissions = result;

    return 0;
}