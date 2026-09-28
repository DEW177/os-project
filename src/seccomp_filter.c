#include <stdint.h>
#include <stdio.h>
#include <sys/prctl.h>

#include <seccomp.h>

#include "sandbox.h"

/*
 * รายการ syscall ที่อนุญาตสำหรับโปรแกรม C แบบ static
 *
 * ห้ามเพิ่ม openat, socket, connect, unlink, clone,
 * execve แบบทั่วไป หรือ syscall ที่ไม่จำเป็น
 */
static const char *const allowed_syscalls[] = {
    "read",
    "write",
    "readv",
    "writev",
    "close",

    "brk",
    "mmap",
    "munmap",
    "mremap",
    "mprotect",
    "madvise",

    "fstat",
    "newfstatat",
    "lseek",
    "readlink",
    "readlinkat",

    "rt_sigaction",
    "rt_sigprocmask",
    "rt_sigreturn",
    "sigaltstack",
    

    "exit",
    "exit_group",

    "arch_prctl",
    "set_tid_address",
    "set_robust_list",
    "rseq",

    "prlimit64",
    "getrandom",
    "futex",

    "clock_gettime",
    "clock_nanosleep",
    "nanosleep",

    "getpid",
    "gettid",
    "sched_yield",

    NULL
};

int apply_seccomp(const char *exe_path)
{
    scmp_filter_ctx context;

    if (exe_path == NULL) {
        return -1;
    }

    /*
     * ห้ามยกระดับสิทธิ์หลังจากนี้
     */
    if (prctl(
            PR_SET_NO_NEW_PRIVS,
            1,
            0,
            0,
            0) < 0) {
        return -1;
    }

    /*
     * syscall ที่ไม่อยู่ใน whitelist จะฆ่า process ทันที
     */
    context = seccomp_init(SCMP_ACT_KILL_PROCESS);

    if (context == NULL) {
        return -1;
    }

    for (size_t i = 0;
         allowed_syscalls[i] != NULL;
         i++) {
        int syscall_number;

        syscall_number =
            seccomp_syscall_resolve_name(
                allowed_syscalls[i]
            );

        /*
         * ถ้า syscall ไม่มีใน architecture นี้
         * ให้ข้ามไป
         */
        if (syscall_number < 0) {
            continue;
        }

        if (seccomp_rule_add(
                context,
                SCMP_ACT_ALLOW,
                syscall_number,
                0) < 0) {
            seccomp_release(context);
            return -1;
        }
    }

    /*
     * อนุญาต execve เฉพาะครั้งแรก
     * โดย argument แรกต้องเป็น pointer เดียวกับ exe_path
     */
    if (seccomp_rule_add(
            context,
            SCMP_ACT_ALLOW,
            SCMP_SYS(execve),
            1,
            SCMP_A0(
                SCMP_CMP_EQ,
                (scmp_datum_t)(uintptr_t)exe_path)) < 0) {
        seccomp_release(context);
        return -1;
    }

    if (seccomp_load(context) < 0) {
        seccomp_release(context);
        return -1;
    }

    seccomp_release(context);
    return 0;
}