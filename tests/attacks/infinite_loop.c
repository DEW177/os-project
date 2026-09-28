/* วนไม่จบ — คาดหวัง TLE (RLIMIT_CPU -> SIGXCPU) */
int main(void) {
    volatile unsigned long i = 0;
    while (1) i++;
}
