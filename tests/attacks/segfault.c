/* เข้าถึง NULL — คาดหวัง RE (SIGSEGV) */
int main(void) {
    volatile int *p = 0;
    *p = 1;
    return 0;
}
