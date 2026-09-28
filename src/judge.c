/*
 * judge.c — วนรันทุก test case แล้วสรุป verdict               [สมาชิก C]
 */
#include <stdio.h>
#include "sandbox.h"

int judge_all(const judge_config *cfg, judge_report *rep) {
    /*
     * TODO(C):
     *  for i = 1, 2, 3, ... จนกว่าจะไม่มี <tests_dir>/<i>.in (หรือครบ MAX_TESTS)
     *    1. สร้าง run_config:
     *         input_file  = <tests_dir>/<i>.in
     *         output_file = <work_dir>/<i>.actual
     *         wall_limit_ms = time_limit_ms * 2
     *    2. run_sandboxed(&rc, &rep->results[i-1])      <- ของ A
     *    3. ถ้า verdict == AC -> check_output(<i>.out, <i>.actual)
     *         ไม่ตรง -> verdict = WA
     *    4. อัปเดต max_time_ms / max_memory_kb
     *    5. ถ้าไม่ผ่าน และ stop_on_fail -> หยุด
     *  final = verdict ของเคสแรกที่ไม่ผ่าน, หรือ AC ถ้าผ่านทุกเคส
     */
    (void)cfg;
    (void)rep;
    fprintf(stderr, "[judge] judge_all(): not implemented yet\n");
    return -1;
}
