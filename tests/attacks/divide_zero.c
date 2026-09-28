#include <stdio.h>

int main(void)
{
    volatile int zero = 0;
    int result = 10 / zero;

    printf("%d\n", result);
    return 0;
}