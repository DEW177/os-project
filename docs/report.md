# รายงานโปรเจค: Secure Code Judge Sandbox

**รายวิชา** Operating Systems and System Calls Programming
**สมาชิก** A: `[ชื่อ–รหัส]` · B: `[ชื่อ–รหัส]` · C: `[ชื่อ–รหัส]`

> ส่วนที่อยู่ใน `[วงเล็บเหลี่ยม]` ให้เติมด้วยข้อมูลจริงหลังทดสอบเสร็จ

---

## 1. บทนำ

### 1.1 ที่มาและปัญหา

ระบบตรวจการบ้านอัตโนมัติ (Online Judge) เช่น Codeforces หรือ Grader ของมหาวิทยาลัย ต้องนำโค้ดที่นักศึกษาส่งมา**รันจริงบนเครื่องของผู้ตรวจ** เพื่อดูว่าตอบถูกและทันเวลาหรือไม่ ปัญหาคือโค้ดเหล่านั้น**ไม่สามารถไว้ใจได้**

- **เขียนผิดโดยไม่ตั้งใจ:** วนลูปไม่จบ, ใช้หน่วยความจำเกิน, พิมพ์ output ไม่หยุด
- **ตั้งใจโจมตี:** ลบไฟล์ของเครื่อง, เชื่อมต่อเครือข่ายเพื่อส่งเฉลยออกไป, สร้าง process จำนวนมากจนเครื่องค้าง (fork bomb)

ถ้ารันโค้ดตรง ๆ ปัญหาเหล่านี้จะกระทบทั้งเครื่อง และทำให้งานตรวจของนักศึกษาคนอื่นหยุดตามไปด้วย

### 1.2 วัตถุประสงค์

1. สร้างโปรแกรม judge ที่ compile และรันโค้ด C พร้อมวัดเวลาและหน่วยความจำได้
2. จำกัดทรัพยากรของโปรแกรมที่รัน และตัดสินผลเป็น verdict มาตรฐาน (AC, WA, TLE, MLE, OLE, RE, CE)
3. ป้องกัน syscall อันตราย และรายงานเป็น Security Violation (SV)
4. ศึกษาและใช้ system call ของ Linux ในการควบคุม process โดยตรง

### 1.3 ขอบเขต

- รองรับภาษา C (compile ด้วย gcc แบบ `-static`)
- ทำงานบน Linux (ทดสอบบน `[Ubuntu xx.xx บน WSL2 / เครื่องจริง]`)
- ใช้งานผ่าน command line และมีหน้าเว็บสำหรับสาธิตการทำงาน

---

## 2. สถาปัตยกรรมระบบ

```
 source.c ─▶ Compiler ─▶ Judge Driver ─▶ Sandbox Runner (ต่อ 1 test case) ─▶ Checker ─▶ Verdict
             (gcc+limit)   (วนทุกเคส)     fork → limit → seccomp → execve        (เทียบ output)
                                           wait4 + alarm
```

| โมดูล | ไฟล์ | หน้าที่ |
|---|---|---|
| CLI | `src/main.c` | รับ argument, สร้างโฟลเดอร์ทำงานชั่วคราว, แสดงผลเป็นตารางหรือ JSON |
| Compiler | `src/compiler.c` | เรียก gcc ใน process แยกที่มีเพดานทรัพยากร |
| Judge Driver | `src/judge.c` | วนรันทุก test case แล้วสรุปผล |
| Sandbox Runner | `src/runner.c` | สร้าง process ลูก ตั้งเพดาน รันโปรแกรม เก็บสถิติ |
| Seccomp Filter | `src/seccomp_filter.c` | สร้างและติดตั้ง whitelist ของ syscall |
| Checker | `src/checker.c` | เทียบ output กับเฉลย |
| Interface กลาง | `include/sandbox.h` | struct และ prototype ที่ทุกโมดูลใช้ร่วมกัน |

ทุกโมดูลคุยกันผ่าน `sandbox.h` เท่านั้น สมาชิกแต่ละคนจึงพัฒนาส่วนของตัวเองไปพร้อมกันได้

---

## 3. System Calls ที่ใช้

| System call | ใช้ที่ไหน | ทำไมต้องใช้ |
|---|---|---|
| `fork` | runner, compiler | แยกโปรแกรมที่ไม่ไว้ใจออกเป็น process ลูก judge (parent) จะได้คอยเฝ้าและไม่พังตาม |
| `execve` | runner, compiler | เปลี่ยน process ลูกให้เป็นโปรแกรมนักศึกษา โดยเพดานและ filter ติดตัวไปด้วย |
| `dup2`, `open` | runner | ต่อ stdin/stdout ของโปรแกรมเข้ากับไฟล์ test case |
| `setrlimit` | runner, compiler | ให้ kernel บังคับเพดาน CPU, หน่วยความจำ, ขนาดไฟล์, stack |
| `prctl(PR_SET_NO_NEW_PRIVS)` | seccomp | ป้องกันการยกระดับสิทธิ์ และจำเป็นก่อนติดตั้ง seccomp |
| `seccomp` | seccomp | กรอง syscall ที่โปรแกรมเรียกได้ นอกรายการ = ถูกฆ่าด้วย `SIGSYS` |
| `alarm`, `sigaction` | runner | จับเวลาจริง (wall time) สำหรับโปรแกรมที่ไม่ใช้ CPU แต่ไม่ยอมจบ |
| `kill` | runner | ฆ่าโปรแกรมทั้ง process group เมื่อหมดเวลา |
| `wait4` | runner, compiler | รอลูกจบ พร้อมได้สถานะการจบ และ `rusage` (เวลา CPU, หน่วยความจำสูงสุด) ในคำสั่งเดียว |

รายละเอียดของแต่ละ syscall อยู่ใน `docs/syscalls.md`

---

## 4. การออกแบบ

### 4.1 ลำดับการตั้งค่าใน process ลูก

```
setpgid → dup2 → chdir → setrlimit → prctl(NO_NEW_PRIVS) → seccomp_load → execve
```

ลำดับนี้สำคัญเพราะ:

- **ทุกอย่างต้องทำก่อน `execve`** เพราะหลัง execve โค้ดของ judge หายไปแล้ว เหลือแต่โปรแกรมนักศึกษา แต่เพดานและ filter ยังติดตัวอยู่
- **`seccomp_load` ต้องเป็นขั้นสุดท้าย** เพราะหลังโหลดแล้ว syscall ที่ runner ใช้ตั้งค่า (เช่น `setrlimit`, `open`) อาจถูกบล็อกไปด้วย

### 4.2 การจำกัดทรัพยากร (สมาชิก A)

| เพดาน | ค่าที่ใช้ | เกินแล้ว | Verdict |
|---|---|---|---|
| `RLIMIT_CPU` | `[ceil(limit)]` วินาที | `SIGXCPU` | TLE |
| `alarm` (wall time) | 2 × time limit | parent ส่ง `SIGKILL` | TLE |
| `RLIMIT_AS` | `[ค่า]` | `malloc` คืน `NULL` | MLE (ตัดสินจาก `ru_maxrss`) |
| `RLIMIT_FSIZE` | `[ค่า]` | `SIGXFSZ` | OLE |
| `RLIMIT_STACK` | `[ค่า]` | `SIGSEGV` | RE |

**ทำไมต้องมีทั้ง RLIMIT_CPU และ alarm** — RLIMIT_CPU นับเฉพาะเวลาที่ใช้ CPU จริง โปรแกรมที่เรียก `sleep()` หรือรอ input จึงไม่ถูกจับ ต้องใช้ alarm ของ parent นับเวลาจริงอีกชั้น

**ทำไมตัดสิน MLE จาก `ru_maxrss`** — `RLIMIT_AS` นับหน่วยความจำเสมือนทั้งหมด ซึ่งมากกว่าที่ใช้จริง และโปรแกรมที่ `malloc` ไม่สำเร็จมักตายด้วย `SIGSEGV` ถ้าดูแค่ signal จะตัดสินผิดเป็น RE จึงต้องเช็ก `ru_maxrss` ก่อนเสมอ

`[อธิบายปัญหาที่เจอระหว่างทำและวิธีแก้]`

### 4.3 Seccomp Whitelist (สมาชิก B)

- ใช้แบบ **whitelist** (default action = `SCMP_ACT_KILL_PROCESS`) แทน blacklist เพราะ Linux มี syscall กว่า 300 ตัว ถ้าใช้ blacklist แล้วลืมตัวใดตัวหนึ่ง (เช่น ห้าม `unlink` แต่ลืม `unlinkat`) ก็จะมีช่องโหว่ทันที
- สร้างรายการด้วยการใช้ `strace -f` กับโปรแกรมตัวอย่างหลายแบบ
- compile โปรแกรมนักศึกษาแบบ `-static` เพื่อลด syscall ตอนเริ่มโปรแกรม (ไม่ต้องเปิดไฟล์ libc)
- `execve` อนุญาตเฉพาะเมื่อ argument แรกเป็น path ของโปรแกรมที่ runner ส่งมาเท่านั้น โปรแกรมจึงเปิด `/bin/sh` ไม่ได้

**Whitelist ที่ใช้จริง** (`[จำนวน]` ตัว)

`[วางรายการ syscall สุดท้าย]`

### 4.4 Compiler และ Checker (สมาชิก C)

- ขั้น compile ก็ต้องมีเพดาน เพราะโค้ดอย่าง `#include "/dev/urandom"` ทำให้ gcc อ่านไฟล์ไม่รู้จบและกินหน่วยความจำทั้งเครื่อง
- Checker ตัด whitespace ท้ายบรรทัดและบรรทัดว่างท้ายไฟล์ก่อนเทียบ เพื่อไม่ให้ตัดสิน WA เพราะช่องว่างเกิน
- Judge หยุดที่ test case แรกที่ไม่ผ่าน (ปรับได้ด้วย `--all`)

`[อธิบายปัญหาที่เจอระหว่างทำและวิธีแก้]`

### 4.5 การแปลงผลเป็น Verdict

เช็กตามลำดับนี้ (เช็ก TLE/MLE ก่อน RE เพราะโปรแกรมที่ชนเพดานมักตายด้วย signal ที่ดูเหมือน RE)

1. หมดเวลาจริง หรือ `SIGXCPU` หรือเวลา CPU เกิน → **TLE**
2. `ru_maxrss` ≥ limit → **MLE**
3. `SIGXFSZ` → **OLE**
4. `SIGSYS` → **SV**
5. signal อื่น หรือ exit code ≠ 0 → **RE**
6. exit code = 0 → ส่งให้ checker → **AC / WA**

---

## 5. การทดสอบ

### 5.1 ชุดทดสอบ

ใช้โจทย์ A+B (3 test case) และโปรแกรมทดสอบ 14 ไฟล์ใน `tests/attacks/` รันทั้งหมดด้วย `make test`

สคริปต์ทดสอบตรวจเพิ่มอีก 3 อย่างนอกจาก verdict:

- ไฟล์ `/tmp/judge_canary.txt` ต้องยังอยู่หลังรัน `delete_file.c`
- ไฟล์ `/tmp/judge_pwned` ต้องไม่ถูกสร้างหลังรัน `system_call_shell.c`
- ต้องไม่มี zombie process เหลือ

### 5.2 ผลการทดสอบ

| ไฟล์ | สิ่งที่ทำ | คาดหวัง | ผลจริง | เวลา | หน่วยความจำ |
|---|---|---|---|---|---|
| `aplusb_correct.c` | โปรแกรมปกติ | AC | `[ ]` | `[ ]` | `[ ]` |
| `aplusb_wrong.c` | คำตอบผิด | WA | `[ ]` | `[ ]` | `[ ]` |
| `infinite_loop.c` | วนไม่จบ | TLE | `[ ]` | `[ ]` | `[ ]` |
| `sleep_forever.c` | sleep(100) | TLE | `[ ]` | `[ ]` | `[ ]` |
| `memory_hog.c` | malloc ไม่หยุด | MLE | `[ ]` | `[ ]` | `[ ]` |
| `output_flood.c` | พิมพ์ไม่หยุด | OLE | `[ ]` | `[ ]` | `[ ]` |
| `stack_overflow.c` | recursion ไม่จบ | RE | `[ ]` | `[ ]` | `[ ]` |
| `divide_zero.c` | หารด้วยศูนย์ | RE | `[ ]` | `[ ]` | `[ ]` |
| `segfault.c` | เข้าถึง NULL | RE | `[ ]` | `[ ]` | `[ ]` |
| `fork_bomb.c` | สร้าง process | SV | `[ ]` | `[ ]` | `[ ]` |
| `open_socket.c` | เปิด socket | SV | `[ ]` | `[ ]` | `[ ]` |
| `delete_file.c` | ลบไฟล์ | SV | `[ ]` | `[ ]` | `[ ]` |
| `system_call_shell.c` | เรียก shell | SV | `[ ]` | `[ ]` | `[ ]` |
| `compile_bomb.c` | ทำให้ gcc ค้าง | CE | `[ ]` | — | — |

**ผลรวม:** `[ผ่าน x / 14]`

`[แนบภาพหน้าจอผล make test]`

### 5.3 หลักฐานจาก kernel

`[วางผลจาก dmesg | grep type=1326 แสดงเลข syscall ที่ถูกบล็อก]`

### 5.4 ความแม่นยำและ overhead

`[เทียบเวลาที่ judge วัดได้กับการรันตรง ๆ ด้วย /usr/bin/time -v — overhead ของ judge ต่อ 1 test case]`

---

## 6. ข้อจำกัดและงานต่อยอด

### ข้อจำกัด

- **Filesystem:** โปรแกรมยังอ่านไฟล์ที่ user ของ judge อ่านได้ ถ้า whitelist มี `open`/`openat` (whitelist ปัจจุบันไม่มี จึงเปิดไฟล์ใหม่ไม่ได้เลย)
- **การวัดหน่วยความจำ:** `ru_maxrss` เป็นค่าสูงสุดที่ kernel เห็น อาจคลาดจากค่าจริงเล็กน้อย
- **ภาษา:** รองรับแค่ C หากเพิ่มภาษาอื่น (เช่น Python) ต้องสร้าง whitelist แยก เพราะ interpreter เรียก syscall มากกว่า
- **รันทีละเคส:** ยังไม่รันหลาย test case พร้อมกัน

### งานต่อยอด

- ใช้ **namespaces** (`CLONE_NEWNET`, `CLONE_NEWPID`, `CLONE_NEWNS`) แยก network และ filesystem ออกจากเครื่องจริง
- ใช้ **cgroups v2** วัดและจำกัดหน่วยความจำได้แม่นกว่า `RLIMIT_AS`
- รองรับหลายภาษา และรันหลาย test case แบบขนาน

---

## 7. การแบ่งงาน

| สมาชิก | หน้าที่ | ไฟล์ที่รับผิดชอบ |
|---|---|---|
| A `[ชื่อ]` | Process & Resource Control | `runner.c` |
| B `[ชื่อ]` | Security & Isolation | `seccomp_filter.c`, `tests/attacks/` |
| C `[ชื่อ]` | Pipeline, Checker, Integration | `main.c`, `compiler.c`, `judge.c`, `checker.c`, `scripts/`, `web/` |

---

## 8. สรุป

โปรเจคนี้แสดงให้เห็นว่า Linux มีเครื่องมือระดับ system call ที่ใช้สร้าง "กรง" ให้โปรแกรมที่ไม่ไว้ใจได้หลายชั้น ได้แก่ `fork` แยก process, `setrlimit` จำกัดทรัพยากร, `seccomp` จำกัดสิ่งที่โปรแกรมขอจาก kernel ได้ และ `wait4` กับ `alarm` เฝ้าดูและเก็บผล เมื่อใช้ร่วมกันแล้ว โปรแกรมที่แย่แค่ไหนก็พังได้แค่ในกรงของตัวเอง โดยไม่กระทบเครื่อง judge

`[เพิ่มสิ่งที่ทีมได้เรียนรู้]`

## อ้างอิง

- Linux man-pages: `fork(2)`, `execve(2)`, `setrlimit(2)`, `wait4(2)`, `alarm(2)`, `sigaction(2)`, `prctl(2)`, `seccomp(2)`
- libseccomp: `seccomp_init(3)`, `seccomp_rule_add(3)`, `seccomp_load(3)`
- Michael Kerrisk, *The Linux Programming Interface*, No Starch Press, 2010
