#include <stdio.h>
#include <stddef.h>
#include "calculator.h"

typedef CalculatorStatus (*CalculatorOperation)(
    double,
    double,
    double *
);

typedef struct {
    char symbol;
    const char *name;
    CalculatorOperation function;
} Command;

static const Command commands[] = {
    { '+', "Сложение", calculator_add },
    { '-', "Вычитание", calculator_subtract },
    { '*', "Умножение", calculator_multiply },
    { '/', "Деление", calculator_divide }
};

static const size_t command_count = sizeof(commands) / sizeof(commands[0]);

static int read_number(double *number) {
    if(scanf("%lf", number) != 1) {
        if(feof(stdin)) {
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
    char operation;
    double a;
    double b;
    double result;

    while (1) {
        printf("\n=== КАЛЬКУЛЯТОР ===\n");
        
        for(size_t i = 0; i < command_count; i++) {
            printf("%c  %s\n", commands[i].symbol, commands[i].name);
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

        const Command *selected_command = NULL;

        for (size_t i = 0; i < command_count; i++) {
            if(commands[i].symbol == operation) {
                selected_command = &commands[i];
                break;
            }
        }

        if(selected_command == NULL) {
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

        CalculatorStatus status = selected_command->function(a, b, &result);

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

    return 0;
}