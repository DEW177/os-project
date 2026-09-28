/* จองหน่วยความจำไม่หยุด — คาดหวัง MLE (RLIMIT_AS + ru_maxrss) */
#include <stdlib.h>
#include <string.h>
int main(void) {
    while (1) {
        char *p = malloc(1 << 20);   /* 1 MB */
        memset(p, 1, 1 << 20);       /* แตะทุกหน้าให้ RSS ขึ้นจริง */
    }
}
