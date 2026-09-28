/*
 * checker.c — เทียบ output ของโปรแกรมกับเฉลย                  [สมาชิก C]
 */
#include <stdio.h>
#include "sandbox.h"

int check_output(const char *expected_file, const char *actual_file) {
    /*
     * TODO(C):
     *  - อ่านทั้งสองไฟล์ทีละบรรทัด
     *  - ตัด whitespace ท้ายบรรทัด (' ', '\t', '\r') และบรรทัดว่างท้ายไฟล์
     *  - ตรงทุกบรรทัด -> 1, ไม่ตรง -> 0, เปิดไฟล์ไม่ได้ -> -1
     */
    (void)expected_file;
    (void)actual_file;
    fprintf(stderr, "[checker] check_output(): not implemented yet\n");
    return -1;
}
