/*
 * ทำให้ gcc อ่านไฟล์ไม่รู้จบ — คาดหวัง CE (limit ของ compiler stage)
 * ⚠ อย่ารันจนกว่า compile_source() จะมี setrlimit + alarm แล้ว
 */
#include "/dev/urandom"
int main(void) { return 0; }
