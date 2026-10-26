#ifndef CALCULATOR_H
#define CALCULATOR_H

typedef enum {
    CALCULATOR_OK,
    CALCULATOR_DIV_ZERO
} CalculatorStatus;

CalculatorStatus calculator_add(double a, double b, double *result);
CalculatorStatus calculator_subtract(double a, double b, double *result);
CalculatorStatus calculator_multiply(double a, double b, double *result);
CalculatorStatus calculator_divide(double a, double b, double *result);

#endif