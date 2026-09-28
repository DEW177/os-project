/*
 * seccomp_filter.c — whitelist syscall ด้วย libseccomp       [สมาชิก B]
 *
 * Syscalls: prctl(PR_SET_NO_NEW_PRIVS), seccomp
 */
#include <stdio.h>
#include "sandbox.h"

/* TODO(B): #include <seccomp.h>  และ <sys/prctl.h> */

/*
 * รายการตั้งต้น — ต้องยืนยันด้วย `strace -f ./prog` กับโปรแกรมที่ compile แบบ -static
 * syscall ใดไม่อยู่ในนี้ = SCMP_ACT_KILL_PROCESS (โปรแกรมโดน SIGSYS -> verdict SV)
 */
static const char *const ALLOWED_SYSCALLS[] = {
    "read", "write", "readv", "writev",
    "brk", "mmap", "munmap", "mremap", "mprotect",
    "fstat", "newfstatat", "lseek",
    "exit", "exit_group",
    "arch_prctl", "set_tid_address", "set_robust_list", "rseq",
    "prlimit64", "getrandom", "futex",
    "clock_gettime", "clock_nanosleep",   /* ให้ sleep() ทำงานได้ แล้วไปโดน TLE แทน */
    NULL
};

/*
 * ตัวอย่างที่ต้อง "ไม่" อนุญาต: socket, connect, unlink, unlinkat, rename,
 * mkdir, chmod, fork, vfork, clone, clone3, kill, ptrace, execve (ครั้งที่ 2)
 */

int apply_seccomp(const char *exe_path) {
    /*
     * TODO(B):
     *  1. prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)
     *  2. ctx = seccomp_init(SCMP_ACT_KILL_PROCESS)
     *  3. วน ALLOWED_SYSCALLS:
     *       seccomp_rule_add(ctx, SCMP_ACT_ALLOW, seccomp_syscall_resolve_name(name), 0)
     *  4. อนุญาต execve เฉพาะเมื่อ argument แรก == exe_path (pointer เดียวกับที่ runner ส่ง)
     *       seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(execve), 1,
     *                        SCMP_A0(SCMP_CMP_EQ, (scmp_datum_t)exe_path))
     *  5. seccomp_load(ctx); seccomp_release(ctx)
     *  คืน 0 = สำเร็จ, -1 = ผิดพลาด
     */
    (void)exe_path;
    (void)ALLOWED_SYSCALLS;
    fprintf(stderr, "[seccomp] apply_seccomp(): not implemented yet\n");
    return -1;
}
