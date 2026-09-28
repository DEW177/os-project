#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "sandbox.h"

static volatile sig_atomic_t runner_timed_out = 0;
static volatile sig_atomic_t runner_pgid = -1;

static void runner_alarm_handler(int signo)
{
    (void)signo;

    runner_timed_out = 1;

    if (runner_pgid > 0) {
        (void)kill((pid_t)runner_pgid, SIGKILL);
        (void)kill(-(pid_t)runner_pgid, SIGKILL);
    }
}

static int set_limit_value(int resource, rlim_t value)
{
    struct rlimit limit;
    limit.rlim_cur = value;
    limit.rlim_max = value;
    return setrlimit(resource, &limit);
}

static unsigned int ms_to_seconds(long ms)
{
    unsigned long long seconds;

    if (ms <= 0) {
        return 1;
    }

    seconds = ((unsigned long long)ms + 999ULL) / 1000ULL;

    if (seconds > 0xffffffffULL) {
        return 0xffffffffU;
    }

    return (unsigned int)seconds;
}

static long timeval_to_ms(const struct timeval *tv)
{
    return (long)(tv->tv_sec * 1000L + tv->tv_usec / 1000L);
}

static void kill_group_and_reap(pid_t pid)
{
    (void)kill(pid, SIGKILL);
    (void)kill(-pid, SIGKILL);

    while (waitpid(pid, NULL, 0) < 0 && errno == EINTR) {
        /* retry */
    }
}

static int apply_resource_limits(const run_config *cfg)
{
    long stack_kb;

    if (cfg->time_limit_ms <= 0 ||
        cfg->memory_limit_kb <= 0 ||
        cfg->output_limit_kb <= 0) {
        return -1;
    }

    if (set_limit_value(
            RLIMIT_CPU,
            (rlim_t)ms_to_seconds(cfg->time_limit_ms)) < 0) {
        return -1;
    }

    if (set_limit_value(
            RLIMIT_AS,
            (rlim_t)cfg->memory_limit_kb * 1024ULL) < 0) {
        return -1;
    }

    if (set_limit_value(
            RLIMIT_FSIZE,
            (rlim_t)cfg->output_limit_kb * 1024ULL) < 0) {
        return -1;
    }

    /*
     * จำกัด stack แยกจาก memory หลัก
     */
    stack_kb = cfg->memory_limit_kb;

    if (stack_kb > 8192) {
        stack_kb = 8192;
    }

    if (stack_kb < 256) {
        stack_kb = 256;
    }

    if (set_limit_value(
            RLIMIT_STACK,
            (rlim_t)stack_kb * 1024ULL) < 0) {
        return -1;
    }

    /*
     * ป้องกัน fork bomb สำรองจาก seccomp
     */
    if (set_limit_value(RLIMIT_NPROC, 1) < 0) {
        return -1;
    }

    if (set_limit_value(RLIMIT_NOFILE, 64) < 0) {
        return -1;
    }

    if (set_limit_value(RLIMIT_CORE, 0) < 0) {
        return -1;
    }

    return 0;
}

static void runner_child(const run_config *cfg)
{
    int input_fd;
    int output_fd;
    int null_fd;
    int fds[3];

    char *const argv[] = {
        (char *)"prog",
        NULL
    };

    char *const envp[] = {
        NULL
    };

    if (setpgid(0, 0) == -1) {
        _exit(126);
    }

    /*
     * เปิดไฟล์ก่อน chdir เพราะ input_file อาจเป็น relative path
     */
    input_fd = open(cfg->input_file, O_RDONLY | O_CLOEXEC);

    if (input_fd < 0) {
        _exit(126);
    }

    output_fd = open(
        cfg->output_file,
        O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
        0600
    );

    if (output_fd < 0) {
        _exit(126);
    }

    null_fd = open("/dev/null", O_WRONLY | O_CLOEXEC);

    if (null_fd < 0) {
        _exit(126);
    }

    if (dup2(input_fd, STDIN_FILENO) < 0 ||
        dup2(output_fd, STDOUT_FILENO) < 0 ||
        dup2(null_fd, STDERR_FILENO) < 0) {
        _exit(126);
    }

    fds[0] = input_fd;
    fds[1] = output_fd;
    fds[2] = null_fd;

    for (int i = 0; i < 3; i++) {
        if (fds[i] > STDERR_FILENO) {
            close(fds[i]);
        }
    }

    if (chdir(cfg->work_dir) < 0) {
        _exit(126);
    }

    /*
     * ต้องตั้ง resource limit ก่อนโหลด seccomp
     */
    if (apply_resource_limits(cfg) < 0) {
        _exit(126);
    }

    /*
     * seccomp ต้องเป็นขั้นตอนสุดท้ายก่อน execve
     */
    if (apply_seccomp(cfg->exe_path) < 0) {
        _exit(126);
    }

    execve(cfg->exe_path, argv, envp);

    _exit(127);
}

int run_sandboxed(const run_config *cfg, run_result *res)
{
    pid_t pid;
    pid_t waited;
    int status = 0;
    struct rusage usage;

    struct sigaction action;
    struct sigaction old_action;
    unsigned int old_alarm;

    if (cfg == NULL || res == NULL ||
        cfg->exe_path == NULL ||
        cfg->input_file == NULL ||
        cfg->output_file == NULL ||
        cfg->work_dir == NULL) {
        return -1;
    }

    memset(res, 0, sizeof(*res));
    res->verdict = SE;
    res->exit_code = -1;

    runner_timed_out = 0;

    pid = fork();

    if (pid < 0) {
        return -1;
    }

    if (pid == 0) {
        runner_child(cfg);
    }

    runner_pgid = pid;
    (void)setpgid(pid, pid);

    action.sa_handler = runner_alarm_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    if (sigaction(SIGALRM, &action, &old_action) < 0) {
        kill_group_and_reap(pid);
        runner_pgid = -1;
        return -1;
    }

    old_alarm = alarm(0);

    alarm(ms_to_seconds(
        cfg->wall_limit_ms > 0
            ? cfg->wall_limit_ms
            : cfg->time_limit_ms * 2
    ));

    do {
        waited = wait4(pid, &status, 0, &usage);

        if (waited < 0 && errno == EINTR) {
            continue;
        }

        break;
    } while (1);

    alarm(0);
    (void)sigaction(SIGALRM, &old_action, NULL);

    if (old_alarm != 0) {
        alarm(old_alarm);
    }

    runner_pgid = -1;

    if (waited < 0) {
        kill_group_and_reap(pid);
        return -1;
    }

    res->cpu_time_ms =
        timeval_to_ms(&usage.ru_utime) +
        timeval_to_ms(&usage.ru_stime);

    /*
     * Linux รายงาน ru_maxrss เป็น KB
     */
    res->memory_kb = usage.ru_maxrss;

    /*
     * ตรวจ TLE ก่อน
     */
    if (runner_timed_out ||
        res->term_signal == SIGXCPU ||
        res->cpu_time_ms > cfg->time_limit_ms) {
        res->verdict = TLE;
        return 0;
    }

    if (WIFSIGNALED(status)) {
        int signal_number = WTERMSIG(status);
        long memory_threshold =
            (cfg->memory_limit_kb * 9L) / 10L;

        res->term_signal = signal_number;

        if (signal_number == SIGSYS) {
            res->verdict = SV;
        } else if (signal_number == SIGXFSZ) {
            res->verdict = OLE;
        } else if (signal_number == SIGXCPU) {
            res->verdict = TLE;
        } else if (cfg->memory_limit_kb > 0 &&
                   res->memory_kb >= memory_threshold) {
            /*
             * malloc fail แล้วตามด้วย SIGSEGV
             * ให้ตัดสินเป็น MLE หาก memory ใกล้ชน limit
             */
            res->verdict = MLE;
        } else {
            res->verdict = RE;
        }

        return 0;
    }

    if (WIFEXITED(status)) {
        res->exit_code = WEXITSTATUS(status);

        if (res->exit_code == 0) {
            res->verdict = AC;
        } else {
            res->verdict = RE;
        }

        return 0;
    }

    res->verdict = SE;
    return 0;
}