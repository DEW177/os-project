/*
 * เรียก shell — คาดหวัง SV (system() ต้อง fork+execve ซึ่งถูกบล็อก)
 * ใช้ touch แทน rm โดยตั้งใจ; scripts/run_all_tests.sh ตรวจว่าไฟล์ไม่ถูกสร้าง
 */
#include <stdlib.h>
int main(void) {
    system("touch /tmp/judge_pwned");
    return 0;
}
