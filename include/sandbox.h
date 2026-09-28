/*
 * sandbox.h — interface กลางของทั้งทีม (ตกลงกันสัปดาห์ที่ 1)
 * แก้ไฟล์นี้ต้องแจ้งทุกคน เพราะกระทบทุกโมดูล
 */
#ifndef SANDBOX_H
#define SANDBOX_H

/* ================================================================
 * Verdict
 * ================================================================ */
typedef enum {
    AC,   /* Accepted                                   */
    WA,   /* Wrong Answer                               */
    CE,   /* Compile Error                              */
    TLE,  /* Time Limit Exceeded (CPU หรือ wall)        */
    MLE,  /* Memory Limit Exceeded                      */
    OLE,  /* Output Limit Exceeded                      */
    RE,   /* Runtime Error                              */
    SV,   /* Security Violation (seccomp -> SIGSYS)     */
    SE    /* System Error (ความผิดพลาดของ judge เอง)    */
} verdict_t;

static inline const char *verdict_str(verdict_t v) {
    static const char *names[] = {"AC","WA","CE","TLE","MLE","OLE","RE","SV","SE"};
    return (v >= AC && v <= SE) ? names[v] : "??";
}

/* ================================================================
 * Runner — สมาชิก A  (src/runner.c)
 * ================================================================ */
typedef struct {
    const char *exe_path;      /* โปรแกรมที่ compile แล้ว               */
    const char *input_file;    /* -> stdin                               */
    const char *output_file;   /* <- stdout                              */
    const char *work_dir;      /* chdir เข้าไปก่อน execve               */
    long time_limit_ms;        /* CPU time                               */
    long wall_limit_ms;        /* เวลาจริง (ปกติ = time_limit * 2)      */
    long memory_limit_kb;
    long output_limit_kb;
} run_config;

typedef struct {
    verdict_t verdict;         /* AC = รันจบปกติ (ยังไม่ได้เทียบ output) */
    long cpu_time_ms;          /* ru_utime + ru_stime                    */
    long memory_kb;            /* ru_maxrss (Linux หน่วย KB)             */
    int  exit_code;            /* ถ้า WIFEXITED                          */
    int  term_signal;          /* ถ้า WIFSIGNALED                        */
} run_result;

/* คืน 0 = รันสำเร็จ (ดู res->verdict), -1 = judge error */
int run_sandboxed(const run_config *cfg, run_result *res);

/* ================================================================
 * Seccomp — สมาชิก B  (src/seccomp_filter.c)
 * เรียกใน child หลัง setrlimit และก่อน execve
 * ================================================================ */
int apply_seccomp(const char *exe_path);

/* ================================================================
 * Compiler / Judge / Checker — สมาชิก C
 * ================================================================ */
typedef struct {
    const char *src_path;
    const char *exe_path;
    const char *log_path;      /* เก็บ stderr ของ gcc                     */
    long time_limit_ms;        /* กัน compile bomb                       */
    long memory_limit_kb;
} compile_config;

/* คืน 0 = สำเร็จ, 1 = CE, -1 = judge error          (src/compiler.c) */
int compile_source(const compile_config *cfg);

/* คืน 1 = ตรงกัน, 0 = ไม่ตรง, -1 = error            (src/checker.c)  */
int check_output(const char *expected_file, const char *actual_file);

#define MAX_TESTS 128

typedef struct {
    const char *tests_dir;     /* มี 1.in 1.out 2.in 2.out ...           */
    const char *exe_path;
    const char *work_dir;
    long time_limit_ms;
    long memory_limit_kb;
    long output_limit_kb;
    int  stop_on_fail;         /* 1 = หยุดที่เคสแรกที่ไม่ผ่าน           */
} judge_config;

typedef struct {
    int        n_tests;
    run_result results[MAX_TESTS];
    verdict_t  final;
    long       max_time_ms;
    long       max_memory_kb;
} judge_report;

/* คืน 0 = สำเร็จ, -1 = judge error                   (src/judge.c)    */
int judge_all(const judge_config *cfg, judge_report *rep);

#endif /* SANDBOX_H */
