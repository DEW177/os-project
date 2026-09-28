/* หารด้วยศูนย์ — คาดหวัง RE (SIGFPE) */
#include <stdio.h>
int main(void) {
    volatile int z = 0;
    printf("%d\n", 1 / z);
    return 0;
}
