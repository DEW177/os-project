/*
 * main.c — CLI ของ judge                                     [สมาชิก C]
 *
 *   ./judge --src a.c --tests tests/problems/aplusb --time 1000 --mem 256
 *
 * บรรทัดสุดท้ายของผลลัพธ์ต้องเป็น "RESULT: <verdict> ..." เสมอ
 * (scripts/run_all_tests.sh อ่านบรรทัดนี้)
 */
 #include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sandbox.h"

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s --src <file.c> --tests <dir> [options]\n"
        "  --time <ms>     CPU time limit ต่อเคส   (default 1000)\n"
        "  --mem <MB>      memory limit            (default 256)\n"
        "  --output <KB>   output limit            (default 65536)\n"
        "  --all           รันทุกเคส (default: หยุดที่เคสแรกที่ไม่ผ่าน)\n"
        "  --json          แสดงผลเป็น JSON\n", prog);
}

static void print_table(const judge_report *rep) {
    for (int i = 0; i < rep->n_tests; i++) {
        const run_result *r = &rep->results[i];
        printf("Test %3d  %-3s  %6ld ms  %8ld KB\n",
               i + 1, verdict_str(r->verdict), r->cpu_time_ms, r->memory_kb);
    }
    printf("RESULT: %s time=%ldms mem=%ldKB\n",
           verdict_str(rep->final), rep->max_time_ms, rep->max_memory_kb);
}

static void print_json(const judge_report *rep) {
    printf("{\"verdict\":\"%s\",\"time_ms\":%ld,\"memory_kb\":%ld,\"tests\":[",
           verdict_str(rep->final), rep->max_time_ms, rep->max_memory_kb);
    for (int i = 0; i < rep->n_tests; i++) {
        const run_result *r = &rep->results[i];
        printf("%s{\"verdict\":\"%s\",\"time_ms\":%ld,\"memory_kb\":%ld}",
               i ? "," : "", verdict_str(r->verdict), r->cpu_time_ms, r->memory_kb);
    }
    printf("]}\n");
}
static void cleanup_work_dir(const char *directory)
{
    DIR *dir;
    struct dirent *entry;

    dir = opendir(directory);

    if (dir == NULL) {
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        char path[PATH_MAX];
        int written;

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        written = snprintf(
            path,
            sizeof(path),
            "%s/%s",
            directory,
            entry->d_name
        );

        if (written < 0 ||
            (size_t)written >= sizeof(path)) {
            continue;
        }

        if (unlink(path) < 0 &&
            errno != ENOENT) {
            perror(path);
        }
    }

    closedir(dir);

    if (rmdir(directory) < 0 &&
        errno != ENOENT) {
        perror(directory);
    }
}

int main(int argc, char **argv) {
    const char *src = NULL, *tests = NULL;
    long time_ms = 1000, mem_mb = 256, out_kb = 65536;
    int run_all = 0, json = 0;

    static const struct option opts[] = {
        {"src",    required_argument, 0, 's'},
        {"tests",  required_argument, 0, 't'},
        {"time",   required_argument, 0, 'T'},
        {"mem",    required_argument, 0, 'm'},
        {"output", required_argument, 0, 'o'},
        {"all",    no_argument,       0, 'a'},
        {"json",   no_argument,       0, 'j'},
        {"help",   no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "s:t:T:m:o:ajh", opts, NULL)) != -1) {
        switch (c) {
        case 's': src     = optarg;       break;
        case 't': tests   = optarg;       break;
        case 'T': time_ms = atol(optarg); break;
        case 'm': mem_mb  = atol(optarg); break;
        case 'o': out_kb  = atol(optarg); break;
        case 'a': run_all = 1;            break;
        case 'j': json    = 1;            break;
        default:  usage(argv[0]); return 2;
        }
    }
    if (!src || !tests) { usage(argv[0]); return 2; }

    /* โฟลเดอร์ทำงานชั่วคราว: เก็บ binary, output, log */
    char work_dir[] = "/tmp/judge.XXXXXX";
    if (!mkdtemp(work_dir)) { perror("mkdtemp"); return 1; }

    char exe[PATH_MAX], log[PATH_MAX];
    snprintf(exe, sizeof exe, "%s/prog", work_dir);
    snprintf(log, sizeof log, "%s/compile.log", work_dir);

    judge_report rep;
    memset(&rep, 0, sizeof rep);

    compile_config cc = {
        .src_path = src, .exe_path = exe, .log_path = log,
        .time_limit_ms = 10000, .memory_limit_kb = 512 * 1024,
    };
    int cr = compile_source(&cc);

    if (cr == 1) {
        rep.final = CE;
    } else if (cr != 0) {
        rep.final = SE;
    } else {
        judge_config jc = {
            .tests_dir = tests, .exe_path = exe, .work_dir = work_dir,
            .time_limit_ms = time_ms, .memory_limit_kb = mem_mb * 1024,
            .output_limit_kb = out_kb, .stop_on_fail = !run_all,
        };
        if (judge_all(&jc, &rep) != 0) rep.final = SE;
    }

    if (json) print_json(&rep);
    else      print_table(&rep);

    cleanup_work_dir(work_dir);
    return rep.final == AC ? 0 : 1;
}
