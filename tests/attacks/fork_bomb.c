/*
 * พยายามสร้าง process — คาดหวัง SV (seccomp บล็อก clone/fork)
 * หมายเหตุ: จำกัดไว้ 64 ครั้งโดยตั้งใจ เพื่อไม่ให้เครื่องค้างระหว่างที่ sandbox ยังทำไม่เสร็จ
 * (fork bomb จริงคือ while(1) fork(); — ห้ามรันนอก VM)
 */
#include <unistd.h>
int main(void) {
    for (int i = 0; i < 64; i++) {
        if (fork() == 0) { sleep(1); _exit(0); }
    }
    return 0;
}
