# รายงานโปรเจค: Secure Code Judge Sandbox

## 1. บทนำและปัญหา
<!-- ทำไมการรันโค้ดที่ไม่ไว้ใจจึงอันตราย, ตัวอย่าง Online Judge -->

## 2. สถาปัตยกรรมระบบ
<!-- แผนภาพ compiler → judge → runner → checker, sandbox.h -->

## 3. System Calls ที่ใช้
| Syscall | ใช้ที่ไหน | ทำไมต้องใช้ |
|---|---|---|
| fork / execve | | |
| setrlimit | | |
| wait4 + rusage | | |
| seccomp / prctl | | |
| alarm / sigaction / kill | | |

## 4. การออกแบบ Sandbox
### 4.1 Resource limits (A)
### 4.2 Seccomp whitelist (B)
### 4.3 Compile stage และ Checker (C)

## 5. ผลการทดสอบ
<!-- วางผลจาก make test, ตาราง verdict ของ attack suite, overhead ของ judge -->

## 6. ข้อจำกัดและงานต่อยอด
<!-- เช่น ยังไม่มี namespace/cgroups, รองรับภาษาเดียว -->

## 7. การแบ่งงาน
| สมาชิก | หน้าที่ |
|---|---|
| A | Process & Resource Control |
| B | Security & seccomp |
| C | Pipeline, Checker, Integration |
