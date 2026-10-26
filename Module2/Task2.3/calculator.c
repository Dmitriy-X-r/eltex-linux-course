#include "calculator.h"

CalculatorStatus calculator_add(double a, double b, double *result) {
    *result = a+b;
    return CALCULATOR_OK;
}

CalculatorStatus calculator_subtract(double a, double b, double *result)
{
    *result = a - b;
    return CALCULATOR_OK;
}

CalculatorStatus calculator_multiply(double a, double b, double *result)
{
    *result = a * b;
    return CALCULATOR_OK;
}

CalculatorStatus calculator_divide(double a, double b, double *result)
{
    if (b == 0.0) {
        return CALCULATOR_DIV_ZERO;
    }

    *result = a / b;
    return CALCULATOR_OK;
}