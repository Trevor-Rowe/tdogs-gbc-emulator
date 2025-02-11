#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>

/* Sanbox Code */

/* Variadic Function */
typedef int (*VariadicOp)(int, ...); // Declare

int sum(int count, ...) { // Define
    va_list args;
    va_start(args, count);
    int total = 0;
    for (int i = 0; i < count; i++)
        total += va_arg(args, int);
    va_end(args);
    return total;
}

void process(VariadicOp func) { // Parameters defined here
    printf("Sum: %d\n", func(1, 2,  3, 4));
}

int variadic_test(int count, ...);

int variadic_test(int count, ...)
{
    va_list args;
    va_start(args, count);

    int total = 0;
    for (int i = 0; i < count; i++)
    {
        total += va_arg(args, int);
    }

    va_end(args);
    return total;
}

uint8_t underflow_testing(uint8_t a, uint8_t b)
{
    return a - b;
}

void shift_testing() 
{
    uint8_t a = 0x80;
    uint8_t b = 0x00;
    printf("Post Shift: %02X\n", ((uint8_t)(a << 1)));
}

int main()
{
    // Extreme case
    // printf("Result 0x00-0xFF: %d\n", underflow_testing(0x00, 0xFF)); // 0x01
    shift_testing();
}