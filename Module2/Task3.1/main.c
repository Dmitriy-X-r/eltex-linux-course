#include <stdio.h>
#include <string.h>

#include "permissions.h"

static int read_line(const char *prompt, char *buffer, size_t size){
    printf("%s", prompt);

    if (fgets(buffer, size, stdin) == NULL) {
        return -1;
    }

    size_t length = strlen(buffer);

    if (length > 0 && buffer[length - 1] == '\n') {
        buffer[length - 1] = '\0';
    } else {
        int ch;

        while ((ch = getchar()) != '\n' && ch != EOF) {}

        return -1;
    }

    return 0;
}

static void print_permissions(mode_t permissions){
    char symbolic[10];
    char binary[10];

    permissions_to_symbolic(permissions, symbolic);
    permissions_to_binary(permissions, binary);

    printf("Буквенное представление: %s\n", symbolic);
    printf("Цифровое представление: %03o\n", (unsigned int)permissions);
    printf("Битовое представление: %s\n", binary);
}

static void run_permissions_session(mode_t permissions){
    char command[32];

    for (;;) {
        printf("\nТекущие права:\n");
        print_permissions(permissions);

        printf("\nВведите команду (например, u+x, g-w, o=r)\n");
        printf("Введите back для возврата в главное меню\n");

        if (read_line("Команда: ", command, sizeof(command)) != 0) {
            printf("\nНе удалось прочитать команду\n");
            return;
        }

        if (strcmp(command, "back") == 0) {
            return;
        }

        if (apply_permission_command(command, &permissions) != 0) {
            printf("Ошибка: некорректная команда\n");
            continue;
        }

        printf("\nРезультат после команды '%s':\n", command);
        print_permissions(permissions);
        printf("Файл на диске не изменён\n");
    }
}


static void run_manual_permissions(void){
    char input[32];
    mode_t permissions;

    if (read_line(
            "Введите права (например, 754 или rwxr-xr--): ",
            input,
            sizeof(input)) != 0) {
        printf("Ошибка чтения ввода\n");
        return;
    }

    if (parse_permissions(input, &permissions) != 0) {
        printf("Ошибка: некорректные права доступа\n");
        return;
    }

    run_permissions_session(permissions);
}

static void run_file_permissions(void)
{
    char filename[256];
    mode_t permissions;

    if (read_line("Введите путь к файлу: ",
                  filename,
                  sizeof(filename)) != 0) {
        printf("Ошибка чтения пути к файлу\n");
        return;
    }

    if (get_file_permissions(filename, &permissions) != 0) {
        printf("Ошибка: не удалось получить права файла '%s'\n",
               filename);
        return;
    }

    printf("\nПрава доступа к файлу '%s':\n", filename);
    print_permissions(permissions);

    run_permissions_session(permissions);
}

int main(void)
{
    char choice[32];

    for (;;) {
        printf("\n=== Калькулятор прав доступа ===\n");
        printf("1. Ввести права вручную\n");
        printf("2. Получить права файла\n");
        printf("0. Выход\n");

        if (read_line("Выберите пункт меню: ",
                      choice,
                      sizeof(choice)) != 0) {
            printf("\nЗавершение программы.\n");
            return 0;
        }

        if (strcmp(choice, "1") == 0) {
            run_manual_permissions();
        } else if (strcmp(choice, "2") == 0) {
            run_file_permissions();
        } else if (strcmp(choice, "0") == 0) {
            printf("Программа завершена\n");
            return 0;
        } else {
            printf("Ошибка: выберите 1, 2 или 0\n");
        }
    }
}