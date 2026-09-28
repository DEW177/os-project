/*
 * ลบไฟล์นอก sandbox — คาดหวัง SV (seccomp บล็อก unlink/unlinkat)
 * scripts/run_all_tests.sh จะสร้างไฟล์ canary ไว้ก่อน แล้วตรวจว่ายังอยู่
 */
#include <unistd.h>
int main(void) {
    unlink("/tmp/judge_canary.txt");
    return 0;
}
