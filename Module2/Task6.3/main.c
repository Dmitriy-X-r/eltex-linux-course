#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <dirent.h>
#include <dlfcn.h>

#include "calculator.h"

#define PLUGIN_DIR "./plugins"
#define PATH_LENGTH 512

typedef CalculatorStatus (*CalculatorOperation)(
    double,
    double,
    double *
);

typedef struct {
    char symbol;
    const char *name;
    const char *filename;
    const char *function_name;
    CalculatorOperation function;
    void *library_handle;
} Command;

static Command commands[] = {
    { '+', "Сложение",  "libadd.so",      "calculator_add",      NULL, NULL },
    { '-', "Вычитание", "libsubtract.so", "calculator_subtract", NULL, NULL },
    { '*', "Умножение", "libmultiply.so", "calculator_multiply", NULL, NULL },
    { '/', "Деление",   "libdivide.so",   "calculator_divide",   NULL, NULL }
};

static const size_t command_count = sizeof(commands) / sizeof(commands[0]);

static int load_plugins(void)
{
    DIR *directory = opendir(PLUGIN_DIR);

    if (directory == NULL) {
        perror("Не удалось открыть каталог плагинов");
        return 0;
    }

    struct dirent *entry;

    while ((entry = readdir(directory)) != NULL) {
        for (size_t i = 0; i < command_count; i++) {
            if (strcmp(entry->d_name, commands[i].filename) != 0) {
                continue;
            }

            char path[PATH_LENGTH];
            int length = snprintf(
                path,
                sizeof(path),
                "%s/%s",
                PLUGIN_DIR,
                entry->d_name
            );

            if (length < 0 || (size_t)length >= sizeof(path)) {
                fprintf(stderr, "Слишком длинный путь к библиотеке\n");
                continue;
            }

            void *handle = dlopen(path, RTLD_NOW);

            if (handle == NULL) {
                fprintf(
                    stderr,
                    "Не удалось загрузить %s: %s\n",
                    path,
                    dlerror()
                );
                continue;
            }

            dlerror();
            void *symbol = dlsym(handle, commands[i].function_name);
            const char *error = dlerror();

            if (error != NULL) {
                fprintf(
                    stderr,
                    "Не найдена функция %s: %s\n",
                    commands[i].function_name,
                    error
                );
                dlclose(handle);
                continue;
            }

            /*
             * На Linux/POSIX адрес функции, полученный через dlsym,
             * можно использовать как указатель на эту функцию.
             * memcpy избегает предупреждения компилятора о прямом
             * преобразовании void * в указатель на функцию.
             */
            if (sizeof(commands[i].function) != sizeof(symbol)) {
                fprintf(stderr, "Несовместимый размер указателя\n");
                dlclose(handle);
                continue;
            }

            memcpy(&commands[i].function, &symbol, sizeof(symbol));
            commands[i].library_handle = handle;
            break;
        }
    }

    closedir(directory);

    return 1;
}

static void unload_plugins(void)
{
    for (size_t i = 0; i < command_count; i++) {
        if (commands[i].library_handle != NULL) {
            dlclose(commands[i].library_handle);
            commands[i].library_handle = NULL;
            commands[i].function = NULL;
        }
    }
}

static int read_number(double *number)
{
    if (scanf("%lf", number) != 1) {
        if (feof(stdin)) {
            return -1;
        }

        int ch;

        while ((ch = getchar()) != '\n' && ch != EOF) {}

        return 0;
    }

    return 1;
}

int main(void)
{
    if (!load_plugins()) {
        return 1;
    }

    char operation;
    double a;
    double b;
    double result;

    while (1) {
        printf("\n=== КАЛЬКУЛЯТОР ===\n");

        int available_count = 0;

        for (size_t i = 0; i < command_count; i++) {
            if (commands[i].function != NULL) {
                printf("%c  %s\n", commands[i].symbol, commands[i].name);
                available_count++;
            }
        }

        if (available_count == 0) {
            printf("Нет доступных операций. Проверьте каталог %s\n", PLUGIN_DIR);
            break;
        }

        printf("q  Выход\n");
        printf("Выберите операцию: ");

        if (scanf(" %c", &operation) != 1) {
            printf("\nВыход из калькулятора\n");
            break;
        }

        if (operation == 'q' || operation == 'Q') {
            printf("Выход из калькулятора\n");
            break;
        }

        Command *selected_command = NULL;

        for (size_t i = 0; i < command_count; i++) {
            if (commands[i].function != NULL &&
                commands[i].symbol == operation) {
                selected_command = &commands[i];
                break;
            }
        }

        if (selected_command == NULL) {
            printf("Ошибка: неизвестная операция\n");
            continue;
        }

        printf("Введите первое число: ");

        int input_status = read_number(&a);

        if (input_status == -1) {
            printf("\nВвод завершён. Выход из программы.\n");
            break;
        }

        if (input_status == 0) {
            printf("Ошибка: необходимо ввести число\n");
            continue;
        }

        printf("Введите второе число: ");

        input_status = read_number(&b);

        if (input_status == -1) {
            printf("\nВвод завершён. Выход из программы.\n");
            break;
        }

        if (input_status == 0) {
            printf("Ошибка: необходимо ввести число.\n");
            continue;
        }

        CalculatorStatus status =
            selected_command->function(a, b, &result);

        if (status == CALCULATOR_DIV_ZERO) {
            printf("Ошибка: попытка деления на ноль\n");
            continue;
        }

        if (status == CALCULATOR_OK) {
            printf("Результат: %.10g\n", result);
        } else {
            printf("Ошибка: неизвестный статус\n");
        }
    }

    unload_plugins();
    return 0;
}
