#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "sandbox.h"

static volatile sig_atomic_t compile_timed_out = 0;
static volatile sig_atomic_t compile_pgid = -1;

static void compile_alarm_handler(int signo)
{
    (void)signo;

    compile_timed_out = 1;

    if (compile_pgid > 0) {
        (void)kill((pid_t)compile_pgid, SIGKILL);
        (void)kill(-(pid_t)compile_pgid, SIGKILL);
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

static void kill_group_and_reap(pid_t pid)
{
    (void)kill(pid, SIGKILL);
    (void)kill(-pid, SIGKILL);

    while (waitpid(pid, NULL, 0) < 0 && errno == EINTR) {
        /* retry */
    }
}

static void compiler_child(const compile_config *cfg)
{
    int log_fd;
    char *const args[] = {
        (char *)"gcc",
        (char *)"-O2",
        (char *)"-static",
        (char *)"-std=gnu11",
        (char *)"-o",
        (char *)cfg->exe_path,
        (char *)cfg->src_path,
        (char *)"-lm",
        NULL
    };

    if (setpgid(0, 0) == -1) {
        _exit(126);
    }

    log_fd = open(
        cfg->log_path,
        O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
        0600
    );

    if (log_fd < 0) {
        _exit(126);
    }

    if (dup2(log_fd, STDOUT_FILENO) < 0 ||
        dup2(log_fd, STDERR_FILENO) < 0) {
        _exit(126);
    }

    if (log_fd > STDERR_FILENO) {
        close(log_fd);
    }

    /*
     * Compile limits
     */
    if (set_limit_value(
            RLIMIT_CPU,
            (rlim_t)ms_to_seconds(cfg->time_limit_ms)) < 0) {
        _exit(126);
    }

    if (set_limit_value(
            RLIMIT_AS,
            (rlim_t)cfg->memory_limit_kb * 1024ULL) < 0) {
        _exit(126);
    }

    /*
     * จำกัดไฟล์ log/output ของ gcc ไม่ให้โตจนเต็มดิสก์
     */
    if (set_limit_value(
            RLIMIT_FSIZE,
            64ULL * 1024ULL * 1024ULL) < 0) {
        _exit(126);
    }

    if (set_limit_value(
            RLIMIT_STACK,
            64ULL * 1024ULL * 1024ULL) < 0) {
        _exit(126);
    }

    if (set_limit_value(RLIMIT_CORE, 0) < 0) {
        _exit(126);
    }

    /*
     * ใช้ execvp เพื่อเรียก gcc
     */
    execvp("gcc", args);

    _exit(127);
}

int compile_source(const compile_config *cfg)
{
    pid_t pid;
    pid_t waited;
    int status = 0;
    int wait_error = 0;
    struct rusage usage;

    struct sigaction action;
    struct sigaction old_action;
    unsigned int old_alarm;

    if (cfg == NULL ||
        cfg->src_path == NULL ||
        cfg->exe_path == NULL ||
        cfg->log_path == NULL ||
        cfg->time_limit_ms <= 0 ||
        cfg->memory_limit_kb <= 0) {
        return -1;
    }

    compile_timed_out = 0;

    pid = fork();

    if (pid < 0) {
        return -1;
    }

    if (pid == 0) {
        compiler_child(cfg);
    }

    compile_pgid = pid;

    /*
     * ให้ parent พยายามกำหนด process group ของ child
     * child เองก็เรียก setpgid อีกครั้งเพื่อกัน race
     */
    (void)setpgid(pid, pid);

    action.sa_handler = compile_alarm_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    if (sigaction(SIGALRM, &action, &old_action) < 0) {
        kill_group_and_reap(pid);
        compile_pgid = -1;
        return -1;
    }

    old_alarm = alarm(0);
    alarm(ms_to_seconds(cfg->time_limit_ms));

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

    compile_pgid = -1;

    if (waited < 0) {
        wait_error = 1;
    }

    if (wait_error) {
        kill_group_and_reap(pid);
        return -1;
    }

    if (compile_timed_out) {
        return 1;
    }

    if (!WIFEXITED(status)) {
        return 1;
    }

    if (WEXITSTATUS(status) != 0) {
        return 1;
    }

    /*
     * gcc exit 0 แต่ไม่มี executable ถือว่าคอมไพล์ไม่สำเร็จ
     */
    if (access(cfg->exe_path, X_OK) != 0) {
        return 1;
    }

    return 0;
}