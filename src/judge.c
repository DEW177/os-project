#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "sandbox.h"

static int make_case_path(
    char *buffer,
    size_t buffer_size,
    const char *directory,
    int number,
    const char *extension
)
{
    int written = snprintf(
        buffer,
        buffer_size,
        "%s/%d.%s",
        directory,
        number,
        extension
    );

    if (written < 0 || (size_t)written >= buffer_size) {
        return -1;
    }

    return 0;
}

int judge_all(const judge_config *cfg, judge_report *rep)
{
    verdict_t first_failure = AC;

    if (cfg == NULL || rep == NULL ||
        cfg->tests_dir == NULL ||
        cfg->exe_path == NULL ||
        cfg->work_dir == NULL) {
        return -1;
    }

    memset(rep, 0, sizeof(*rep));
    rep->final = AC;

    for (int number = 1;
         number <= MAX_TESTS;
         number++) {
        char input_file[4096];
        char expected_file[4096];
        char actual_file[4096];

        run_config run_cfg;
        run_result result;
        int check_result;

        if (make_case_path(
                input_file,
                sizeof(input_file),
                cfg->tests_dir,
                number,
                "in") < 0) {
            return -1;
        }

        if (make_case_path(
                expected_file,
                sizeof(expected_file),
                cfg->tests_dir,
                number,
                "out") < 0) {
            return -1;
        }

        /*
         * ไม่มี input เคสถัดไปแล้ว
         */
        if (access(input_file, F_OK) != 0) {
            if (errno == ENOENT && number > 1) {
                break;
            }

            return -1;
        }

        if (access(expected_file, F_OK) != 0) {
            return -1;
        }

        if (make_case_path(
                actual_file,
                sizeof(actual_file),
                cfg->work_dir,
                number,
                "actual") < 0) {
            return -1;
        }

        memset(&run_cfg, 0, sizeof(run_cfg));

        run_cfg.exe_path = cfg->exe_path;
        run_cfg.input_file = input_file;
        run_cfg.output_file = actual_file;
        run_cfg.work_dir = cfg->work_dir;
        run_cfg.time_limit_ms = cfg->time_limit_ms;
        run_cfg.wall_limit_ms = cfg->time_limit_ms * 2;
        run_cfg.memory_limit_kb = cfg->memory_limit_kb;
        run_cfg.output_limit_kb = cfg->output_limit_kb;

        if (run_sandboxed(&run_cfg, &result) < 0) {
            return -1;
        }

        /*
         * ถ้าโปรแกรมจบปกติ ค่อยตรวจ output
         */
        if (result.verdict == AC) {
            check_result = check_output(
                expected_file,
                actual_file
            );

            if (check_result < 0) {
                return -1;
            }

            if (check_result == 0) {
                result.verdict = WA;
            }
        }

        rep->results[number - 1] = result;
        rep->n_tests = number;

        if (result.cpu_time_ms > rep->max_time_ms) {
            rep->max_time_ms = result.cpu_time_ms;
        }

        if (result.memory_kb > rep->max_memory_kb) {
            rep->max_memory_kb = result.memory_kb;
        }

        if (result.verdict != AC &&
            first_failure == AC) {
            first_failure = result.verdict;
        }

        if (result.verdict != AC &&
            cfg->stop_on_fail) {
            break;
        }
    }

    if (rep->n_tests == 0) {
        return -1;
    }

    rep->final = first_failure;
    return 0;
}