/* recursion ไม่จบ — คาดหวัง RE (SIGSEGV จาก RLIMIT_STACK) */
int f(int n) {
    volatile char buf[1024];
    buf[0] = (char)n;
    return f(n + 1) + buf[0];
}
int main(void) { return f(0); }
