# Secure Code Judge Sandbox — แผนโปรเจคและการแบ่งหน้าที่

> รายวิชา Operating Systems and System Calls Programming · ทีม 3 คน
> โจทย์หลัก: **"จะรันโค้ดที่ไม่ไว้ใจได้อย่างไร โดยไม่ให้เครื่องพัง"**

---

## 1. ภาพรวมระบบ

ระบบรับไฟล์ซอร์สโค้ด (C/C++) + ชุด test case → compile → รันแต่ละเคสในกรง (sandbox) → วัดเวลา/หน่วยความจำ → เทียบผลลัพธ์ → ออก verdict

```
            ┌──────────────┐     ┌───────────────┐     ┌──────────────────────────┐     ┌────────────┐
 source.c ─▶│  Compiler    │────▶│  Judge Driver │────▶│  Sandbox Runner (1 เคส)  │────▶│  Checker   │──▶ Verdict
            │ (gcc+limits) │ CE  │ วนทุก test    │     │ fork → limits → seccomp  │     │ diff output│   (JSON)
            └──────────────┘     └───────────────┘     │ → execve / wait4 + alarm │     └────────────┘
                                                       └──────────────────────────┘
```

### Flow ของ Sandbox Runner (หัวใจของโปรเจค)

```
Parent                                   Child
──────                                   ─────
fork() ─────────────────────────────────▶ dup2(input.txt → stdin, output.txt → stdout)
                                          chdir(sandbox_dir)
                                          setrlimit(CPU, AS, FSIZE, NPROC, NOFILE, STACK)
                                          prctl(PR_SET_NO_NEW_PRIVS, 1)
                                          seccomp_load(whitelist)
                                          execve("./prog", ...)
alarm(wall_limit)  ← กัน sleep()/รอ input
wait4(pid, &status, 0, &rusage)
  ├─ SIGALRM มาก่อน → kill(pid, SIGKILL) → TLE
  ├─ WIFSIGNALED: SIGXCPU/SIGKILL → TLE, SIGSYS → SV, SIGXFSZ → OLE, SIGSEGV/SIGFPE → RE
  ├─ WIFEXITED: exit code ≠ 0 → RE
  └─ ใช้ rusage: ru_utime + ru_stime = CPU time, ru_maxrss (KB) = memory
```

### Verdict ที่ระบบรองรับ

| Verdict | ความหมาย | ตรวจจับจาก |
|---|---|---|
| **AC** | Accepted | exit 0 + output ตรง |
| **WA** | Wrong Answer | exit 0 แต่ output ไม่ตรง |
| **CE** | Compile Error | gcc exit ≠ 0 |
| **TLE** | Time Limit Exceeded | CPU time เกิน (`SIGXCPU`) หรือ wall time เกิน (`SIGALRM` → kill) |
| **MLE** | Memory Limit Exceeded | `ru_maxrss` เกิน limit / malloc fail ภายใต้ `RLIMIT_AS` |
| **OLE** | Output Limit Exceeded | `SIGXFSZ` จาก `RLIMIT_FSIZE` |
| **RE** | Runtime Error | `SIGSEGV`, `SIGFPE`, `SIGABRT`, exit code ≠ 0 |
| **SV** | Security Violation | `SIGSYS` จาก seccomp (เรียก syscall ต้องห้าม) |

### Syscall ที่ใช้ และใช้ทำอะไร

| Syscall / API | ใช้ทำอะไรในโปรเจค |
|---|---|
| `fork` / `execve` | สร้าง process ลูกแยกจาก judge แล้วแทนที่ด้วยโปรแกรมนักศึกษา |
| `dup2`, `open`, `chdir` | redirect stdin/stdout ไปที่ไฟล์ test, ย้ายไปทำงานใน dir ชั่วคราว |
| `setrlimit` | จำกัด CPU, memory (`RLIMIT_AS`), ขนาดไฟล์ output, จำนวน process (กัน fork bomb), file descriptor, stack |
| `prctl(PR_SET_NO_NEW_PRIVS)` | จำเป็นก่อนโหลด seccomp แบบไม่ใช่ root |
| `seccomp` (ผ่าน libseccomp) | whitelist syscall — เรียกนอกลิสต์ = kill ทันที (`SIGSYS`) |
| `alarm` + `sigaction(SIGALRM)` | จับเวลา wall-clock ฝั่ง parent |
| `kill` | ฆ่า process ลูกเมื่อหมดเวลา |
| `wait4` | รอลูกจบ + ดึง `struct rusage` (เวลา CPU, max RSS) |

---

## 2. การแบ่งหน้าที่

แบ่งตาม "ชั้น" ของระบบ ให้แต่ละคนมีงาน syscall ของตัวเองชัดเจน (อาจารย์มักถามรายคน)

### 👤 สมาชิก A — Process & Resource Control (Sandbox Core)

รับผิดชอบ: `runner.c` — ส่วนที่ fork/exec/วัดผล

- ออกแบบ `struct run_config` / `struct run_result` (ร่วมกับทีมในสัปดาห์แรก)
- `fork` → child: `dup2` stdin/stdout/stderr, `chdir`, `setrlimit` ทั้งหมด → `execve`
- parent: ตั้ง `alarm` + `sigaction(SIGALRM)`, `wait4` เก็บ `rusage`
- แปลง `status` + `rusage` → verdict (TLE/MLE/OLE/RE)
- จัดการ edge case: `EINTR` ตอน `wait4`, zombie, ฆ่า process group (`setpgid` + `kill(-pgid)`)
- จุดโหลด seccomp: เว้น hook ไว้ให้ B เสียบ (`apply_seccomp()` ก่อน `execve`)

**Syscall ที่ต้องอธิบายได้:** `fork`, `execve`, `dup2`, `setrlimit`, `alarm`, `sigaction`, `kill`, `wait4`

### 👤 สมาชิก B — Security & Isolation (seccomp)

รับผิดชอบ: `seccomp_filter.c` + ชุดโค้ดโจมตี

- ศึกษา syscall ที่โปรแกรม C ทั่วไปใช้ ด้วย `strace -f ./prog` → สร้าง **whitelist** (ปลอดภัยกว่า blacklist)
  - ตัวอย่างที่ต้องอนุญาต: `read`, `write`, `brk`, `mmap`, `munmap`, `mprotect`, `fstat`/`newfstatat`, `lseek`, `exit`, `exit_group`, `arch_prctl`, `set_tid_address`, `set_robust_list`, `rseq`, `prlimit64`, `getrandom`, `futex`
  - `execve`: อนุญาตเฉพาะครั้งแรก — ใช้เงื่อนไข argument `SCMP_A0(SCMP_CMP_EQ, path_ptr)`
- ต้องห้าม (ตัวอย่าง): `socket`, `connect`, `unlink`, `unlinkat`, `rename`, `mkdir`, `chmod`, `fork`/`clone`, `kill`, `ptrace`
- แนะนำ compile โปรแกรมนักศึกษาแบบ `-static` → syscall ตอน startup น้อยลง, whitelist แคบลง
- `prctl(PR_SET_NO_NEW_PRIVS)` + `seccomp_load()` ด้วย default action `SCMP_ACT_KILL_PROCESS`
- เขียน **malicious test suite** (ดูหัวข้อ 4) และพิสูจน์ว่าทุกตัวโดนจับ
- (Bonus) sandbox ไฟล์ระบบ: รันใน temp dir ว่าง / `chroot` / namespace

**Syscall ที่ต้องอธิบายได้:** `seccomp`, `prctl`, BPF filter ทำงานยังไง, ทำไม `SIGSYS`

### 👤 สมาชิก C — Judge Pipeline, Checker & Integration

รับผิดชอบ: `compiler.c`, `judge.c`, `checker.c`, `main.c`, การทดสอบ, รายงาน

- **Compiler stage:** เรียก `gcc` ผ่าน `fork`/`execve` เหมือนกัน แต่มี limit (เวลา ~10s, memory, output) กัน compile bomb เช่น `#include "/dev/random"` → CE
- **Judge driver:** วนทุก test case ใน `tests/` (`1.in`/`1.out`, …) เรียก runner ของ A
- **Checker:** เทียบ output (ตัด whitespace ท้ายบรรทัด/ท้ายไฟล์), หยุดที่เคสแรกที่ fail หรือรันทุกเคส (option)
- **CLI + Output:** `./judge --src a.c --tests tests/ --time 1000 --mem 256` → พิมพ์ตาราง + JSON
- ตั้ง `Makefile`, โครง repo, สคริปต์ `run_all_tests.sh` (regression test)
- ประสานงาน integration, เขียนรายงาน + เตรียมสไลด์/เดโม

**Syscall ที่ต้องอธิบายได้:** `fork`/`execve` ของ compiler, `pipe` (ถ้าอ่าน stderr ของ gcc), `open`/`read` ในการเทียบไฟล์

> **ทุกคน** ต้องอธิบาย flow ภาพรวมในหัวข้อ 1 ได้ และต้องรีวิว PR ของอีก 1 คน

---

## 3. Timeline (8 สัปดาห์ — ปรับตามวันส่งจริง)

| สัปดาห์ | A (Process) | B (Security) | C (Pipeline) | Milestone |
|---|---|---|---|---|
| 1 | ศึกษา fork/exec/wait4, ร่าง struct | ศึกษา seccomp, ติดตั้ง libseccomp | ตั้ง repo, Makefile, โครงโฟลเดอร์ | **ตกลง interface (`sandbox.h`)** |
| 2 | runner พื้นฐาน: fork + dup2 + execve + wait4 | `strace` หา syscall ขั้นต่ำ, filter ตัวแรก | compiler stage + CE | รันโปรแกรม hello world ได้ |
| 3 | setrlimit ครบ + วัด time/mem | whitelist v1 + `execve` เงื่อนไข arg | checker + test case ตัวอย่าง | **Demo ภายใน: AC/WA/CE/RE** |
| 4 | alarm/SIGALRM + kill, แยก TLE CPU vs wall | malicious suite ชุดแรก | judge driver วนหลายเคส | TLE/MLE/OLE ทำงาน |
| 5 | จัด edge case (process group, EINTR) | เสียบ seccomp เข้า runner → SV | CLI + JSON output | **Integration ครบทุก verdict** |
| 6 | ปรับความแม่นยำการวัด, benchmark overhead | ทดสอบ bypass, ปิดช่องโหว่ | regression script, รวม test ทั้งหมด | Feature freeze |
| 7 | (Bonus) cgroups / หลายภาษา | (Bonus) chroot/namespace | รายงาน + สไลด์ | Bug fix อย่างเดียว |
| 8 | ซ้อมเดโม | ซ้อมเดโม | ซ้อมเดโม | **ส่งงาน + นำเสนอ** |

---

## 4. แผนทดสอบ (สำคัญมาก — ใช้เป็นเดโมได้เลย)

เก็บไว้ที่ `tests/attacks/` แต่ละไฟล์มี verdict ที่คาดหวัง

| ไฟล์ | สิ่งที่ทำ | คาดหวัง | ป้องกันด้วย |
|---|---|---|---|
| `infinite_loop.c` | `while(1);` | TLE | `RLIMIT_CPU` |
| `sleep_forever.c` | `sleep(100)` | TLE | `alarm` (wall time) |
| `memory_hog.c` | `malloc` วนไม่หยุด | MLE | `RLIMIT_AS` |
| `stack_overflow.c` | recursion ไม่จบ | RE / MLE | `RLIMIT_STACK` |
| `output_flood.c` | `printf` วนไม่หยุด | OLE | `RLIMIT_FSIZE` |
| `fork_bomb.c` | `while(1) fork();` | SV | seccomp (+ `RLIMIT_NPROC`) |
| `open_socket.c` | `socket(AF_INET, …)` | SV | seccomp |
| `delete_file.c` | `unlink("important.txt")` | SV | seccomp |
| `system_call_shell.c` | `system("rm -rf ~")` | SV | seccomp (ห้าม fork/execve ซ้ำ) |
| `divide_zero.c` | `1/0` | RE | `SIGFPE` |
| `segfault.c` | เข้าถึง NULL | RE | `SIGSEGV` |
| `compile_bomb.c` | `#include "/dev/random"` | CE | limit ของ compiler stage |
| `aplusb_correct.c` / `aplusb_wrong.c` | โปรแกรมปกติ | AC / WA | checker |

เกณฑ์ผ่าน: ทุกไฟล์ได้ verdict ตรง, เครื่อง host ไม่ค้าง, ไม่มี zombie เหลือ (`ps`), ไฟล์นอก sandbox ไม่ถูกแตะ

---

## 5. โครงสร้าง Repo

```
Os_project/
├── include/
│   └── sandbox.h        # struct run_config, run_result, enum verdict  (ทุกคนใช้ร่วม)
├── src/
│   ├── main.c           # CLI                                  (C)
│   ├── compiler.c       # compile ด้วย gcc + limits            (C)
│   ├── judge.c          # วนทุก test case                      (C)
│   ├── checker.c        # เทียบ output                         (C)
│   ├── runner.c         # fork/setrlimit/execve/wait4/alarm     (A)
│   └── seccomp_filter.c # whitelist + load filter               (B)
├── tests/
│   ├── problems/aplusb/ # 1.in 1.out 2.in 2.out ...
│   └── attacks/         # โค้ดโจมตีตามหัวข้อ 4                  (B)
├── scripts/run_all_tests.sh                                     (C)
├── docs/report.md
└── Makefile             # gcc -Wall -O2 ... -lseccomp
```

### Interface กลาง (ตกลงกันสัปดาห์ที่ 1)

```c
typedef enum { AC, WA, CE, TLE, MLE, OLE, RE, SV, SE /* system error */ } verdict_t;

typedef struct {
    const char *exe_path;
    const char *input_file, *output_file, *work_dir;
    long time_limit_ms;      // CPU time
    long wall_limit_ms;      // ปกติ = time_limit * 2
    long memory_limit_kb;
    long output_limit_kb;
} run_config;

typedef struct {
    verdict_t verdict;
    long cpu_time_ms;        // ru_utime + ru_stime
    long memory_kb;          // ru_maxrss
    int  exit_code, signal;
} run_result;

int run_sandboxed(const run_config *cfg, run_result *res);   // A
int apply_seccomp(const char *exe_path);                      // B
```

---

## 6. ข้อควรระวังทางเทคนิค

- **ต้องรันบน Linux จริง** (Ubuntu / WSL2) — seccomp, `wait4`, `rusage` ไม่มีบน Windows/macOS แบบเดียวกัน
- ติดตั้ง: `sudo apt install build-essential libseccomp-dev strace`
- `RLIMIT_CPU` หน่วยเป็น **วินาที** → ตั้ง soft = ceil(limit), แล้วเทียบ `rusage` แบบ ms เองอีกชั้น
- `RLIMIT_AS` นับ virtual memory → ตั้งเผื่อไว้ แล้วตัดสิน MLE จาก `ru_maxrss` จะแม่นกว่า
- `ru_maxrss` บน Linux เป็น **KB**
- `wait4` อาจโดน `SIGALRM` ขัด → คืน `EINTR` ต้องวน retry
- ลำดับใน child ต้องเป็น: `setrlimit` → `NO_NEW_PRIVS` → `seccomp_load` → `execve` (โหลด filter ก่อน = syscall ที่ใช้ตั้งค่าอื่นหลังจากนี้จะโดนบล็อก)
- ห้ามทดสอบ fork bomb โดยไม่มี `RLIMIT_NPROC` — ทดสอบใน VM ก่อน

---

## 7. งาน Bonus (ถ้ามีเวลา)

- ใช้ **cgroups v2** วัด/จำกัด memory แม่นกว่า `RLIMIT_AS`
- **namespaces** (`unshare` / `clone(CLONE_NEWNET|CLONE_NEWPID|CLONE_NEWNS)`) แยก network และ filesystem
- รองรับหลายภาษา (Python ต้องมี whitelist แยก)
- รันหลาย test case พร้อมกัน (parallel judge)
- Web UI เล็ก ๆ ให้ส่งโค้ด

---

## 8. การทำงานร่วมกัน

- Git: branch ต่อคน (`feat/runner`, `feat/seccomp`, `feat/pipeline`) → PR เข้า `main` + มีคน review 1 คน
- ประชุมสั้น 2 ครั้ง/สัปดาห์: อัปเดตความคืบหน้า + ปัญหาที่ติด
- Definition of Done ของแต่ละงาน: compile ผ่านไม่มี warning, มี test case, `run_all_tests.sh` ผ่านทั้งหมด

## 9. สิ่งที่ต้องส่ง

- [ ] Source code + Makefile + README วิธีรัน
- [ ] ชุด test case (ปกติ + attacks) พร้อมผลลัพธ์
- [ ] รายงาน: สถาปัตยกรรม, อธิบาย syscall แต่ละตัว, ผลทดสอบ, ข้อจำกัด
- [ ] สไลด์ + เดโมสด (แนะนำโชว์: fork bomb / `rm` / socket โดนบล็อก แต่เครื่องไม่พัง)
