/* พิมพ์ไม่หยุด — คาดหวัง OLE (RLIMIT_FSIZE -> SIGXFSZ) */
#include <stdio.h>
int main(void) {
    while (1) puts("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
}
