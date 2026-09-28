/* ไม่ใช้ CPU แต่ไม่จบ — คาดหวัง TLE (ต้องจับด้วย alarm/wall time ไม่ใช่ RLIMIT_CPU) */
#include <unistd.h>
int main(void) {
    sleep(100);
    return 0;
}
