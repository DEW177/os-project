/*
 * compiler.c — compile ซอร์สโค้ดด้วย gcc แบบมี limit          [สมาชิก C]
 *
 * Syscalls: fork, execve, dup2, setrlimit, alarm, wait4
 */
#include <stdio.h>
#include "sandbox.h"

int compile_source(const compile_config *cfg) {
    /*
     * TODO(C):
     *  1. fork
     *  2. child: dup2 stderr -> cfg->log_path
     *            setrlimit(RLIMIT_CPU / RLIMIT_AS / RLIMIT_FSIZE)  กัน compile bomb
     *            execvp("gcc", {"gcc", "-O2", "-static", "-std=gnu11",
     *                           "-o", exe_path, src_path, "-lm", NULL})
     *  3. parent: alarm(time_limit) + wait4
     *  4. exit 0 -> return 0,  อย่างอื่น (รวมโดน kill) -> return 1 (CE)
     *
     * หมายเหตุ: ใช้ -static เพื่อลด syscall ตอนโหลด libc -> whitelist ของ B แคบลง
     */
    (void)cfg;
    fprintf(stderr, "[compiler] compile_source(): not implemented yet\n");
    return -1;
}
