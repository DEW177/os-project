/*
 * runner.c — รันโปรแกรม 1 เคสใน sandbox แล้ววัดผล            [สมาชิก A]
 *
 * Syscalls: fork, dup2, chdir, setrlimit, execve, alarm, sigaction, kill, wait4
 */
#include <stdio.h>
#include "sandbox.h"

int run_sandboxed(const run_config *cfg, run_result *res) {
    /*
     * TODO(A):
     *
     * ---- ใน child (pid == 0) ----
     *  1. setpgid(0, 0)                  ให้ลูกเป็น process group ใหม่ (kill ทั้งกลุ่มได้)
     *  2. open + dup2  input_file  -> STDIN_FILENO
     *     open + dup2  output_file -> STDOUT_FILENO  (stderr -> /dev/null)
     *  3. chdir(cfg->work_dir)
     *  4. setrlimit:
     *       RLIMIT_CPU    = ceil(time_limit_ms / 1000)   (หน่วยวินาที!)
     *       RLIMIT_AS     = memory_limit_kb * 1024 (+ เผื่อ)
     *       RLIMIT_FSIZE  = output_limit_kb * 1024
     *       RLIMIT_STACK  = memory_limit_kb * 1024
     *       RLIMIT_NPROC  = 1 หรือค่าน้อย ๆ
     *       RLIMIT_NOFILE = ค่าน้อย ๆ
     *  5. apply_seccomp(cfg->exe_path)   <- ของ B  (ต้องเป็นขั้นตอนสุดท้ายก่อน execve)
     *  6. execve(cfg->exe_path, argv, envp_ว่าง)
     *  7. ถ้า execve กลับมา = error -> _exit(127)
     *
     * ---- ใน parent ----
     *  1. sigaction(SIGALRM, handler) แล้ว alarm(ceil(wall_limit_ms / 1000))
     *     handler: kill(-child_pid, SIGKILL) และตั้ง flag timed_out = 1
     *  2. wait4(pid, &status, 0, &ru)   วน retry ถ้าได้ EINTR
     *  3. alarm(0)
     *  4. cpu_time_ms = ru_utime + ru_stime,  memory_kb = ru_maxrss
     *  5. ตัดสิน verdict:
     *       timed_out หรือ SIGXCPU หรือ cpu_time > limit  -> TLE
     *       SIGSYS                                      -> SV
     *       SIGXFSZ                                     -> OLE
     *       memory_kb > memory_limit_kb                 -> MLE
     *       SIGSEGV / SIGFPE / SIGABRT / exit_code != 0 -> RE
     *       exit 0                                      -> AC  (checker ของ C จะเทียบ output ต่อ)
     */
    (void)cfg;
    (void)res;
    fprintf(stderr, "[runner] run_sandboxed(): not implemented yet\n");
    return -1;
}
