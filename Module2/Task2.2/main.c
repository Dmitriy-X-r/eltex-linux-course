#include <stdio.h>
#include "calculator.h"

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
        printf("+  Сложение\n");
        printf("-  Вычитание\n");
        printf("*  Умножение\n");
        printf("/  Деление\n");
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

        if (operation != '+' &&
            operation != '-' &&
            operation != '*' &&
            operation != '/') {
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

        CalculatorStatus status;

        switch (operation) {
            case '+':
                status = calculator_add(a, b, &result);
                break;

            case '-':
                status = calculator_subtract(a, b, &result);
                break;

            case '*':
                status = calculator_multiply(a, b, &result);
                break;

            case '/':
                status = calculator_divide(a, b, &result);
                break;

            default:
                continue;
        }

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