#include "calculator.h"

CalculatorStatus calculator_multiply(double a, double b, double *result)
{
    *result = a * b;
    return CALCULATOR_OK;
}
