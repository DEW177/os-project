# คู่มือ System Calls สำหรับทีม

อ่านก่อนเริ่มเขียนโค้ดตาม TODO ในไฟล์ของตัวเอง เอกสารนี้อธิบายว่า syscall แต่ละตัว **ทำอะไร คืนค่าอะไร และมีกับดักตรงไหน** ส่วนการประกอบเป็นโปรแกรมให้แต่ละคนเขียนเอง จะได้อธิบายได้ตอนอาจารย์ถาม

อ่านเพิ่มเติมได้ด้วย `man 2 <ชื่อ>` เช่น `man 2 wait4` หรือ `man 2 setrlimit`

---

## ภาพรวม: ใครใช้อะไร

| Syscall | ใช้ในไฟล์ | เจ้าของ |
|---|---|---|
| `fork`, `execve`, `dup2`, `chdir`, `setpgid` | `runner.c`, `compiler.c` | A, C |
| `setrlimit` | `runner.c`, `compiler.c` | A, C |
| `alarm`, `sigaction`, `kill` | `runner.c`, `compiler.c` | A, C |
| `wait4` + `struct rusage` | `runner.c`, `compiler.c` | A, C |
| `prctl`, `seccomp` (ผ่าน libseccomp) | `seccomp_filter.c` | B |
| `open`, `read` | `checker.c` | C |

---

## 1. `fork()` — แบ่ง process เป็นสอง

```c
pid_t fork(void);
```

- เรียกครั้งเดียว แต่**กลับมาสองครั้ง** ครั้งหนึ่งใน parent และอีกครั้งใน child
- ค่าที่คืน:
  - `0` → กำลังอยู่ใน **child**
  - `> 0` → กำลังอยู่ใน **parent** และค่านั้นคือ pid ของ child
  - `-1` → สร้างไม่สำเร็จ ดูสาเหตุจาก `errno`
- child ได้สำเนาทุกอย่างของ parent: หน่วยความจำ, file descriptor ที่เปิดอยู่, signal handler

**กับดัก**

- ใน child ถ้ามีอะไรผิดพลาด ให้จบด้วย `_exit()` **ไม่ใช่** `exit()` เพราะ `exit()` จะ flush buffer ของ stdio ที่ copy มาจาก parent ทำให้ข้อความพิมพ์ซ้ำ
- `printf` ก่อน `fork` ที่ยังไม่ขึ้นบรรทัดใหม่อาจถูกพิมพ์สองรอบ ให้ `fflush(stdout)` ก่อน fork

## 2. `execve()` — เปลี่ยน process ให้เป็นโปรแกรมอื่น

```c
int execve(const char *path, char *const argv[], char *const envp[]);
```

- แทนที่โค้ดและหน่วยความจำของ process ปัจจุบันด้วยโปรแกรมที่ `path` แต่ **pid เดิม**
- ถ้าสำเร็จจะ**ไม่กลับมาเลย** ถ้ากลับมาแปลว่าล้มเหลว (คืน `-1`)
- สิ่งที่**ติดตัวข้าม execve ไป**: pid, file descriptor ที่ไม่ได้ตั้ง `FD_CLOEXEC`, resource limit จาก `setrlimit`, seccomp filter, สถานะ `NO_NEW_PRIVS`
- สิ่งที่**หายไป**: signal handler ที่ตั้งไว้ (ถูกรีเซ็ตเป็นค่า default)

> นี่คือเหตุผลที่ต้องตั้งทุกอย่าง**ก่อน** execve กฎติดตัวไปด้วย แต่โค้ดของเราไม่ได้ไปด้วย

- `envp` ส่งเป็น array ว่าง (`{NULL}`) ได้ จะได้ไม่รั่วตัวแปร environment ของเครื่องให้โปรแกรมนักศึกษา
- `compiler.c` ใช้ `execvp` ได้ เพราะต้องหา `gcc` จาก `PATH`

## 3. `dup2()` + `open()` — ต่อ stdin/stdout เข้ากับไฟล์

```c
int dup2(int oldfd, int newfd);
```

- ทำให้ `newfd` ชี้ไปที่เดียวกับ `oldfd` เช่น เปิดไฟล์ `1.in` ได้ fd 3 แล้ว `dup2(3, 0)` → stdin ของโปรแกรมจะอ่านจาก `1.in`
- fd มาตรฐาน: `0` = stdin, `1` = stdout, `2` = stderr
- หลัง dup2 แล้ว ให้ `close()` fd เดิมทิ้ง จะได้ไม่มี fd เกินค้างอยู่
- ไฟล์ output เปิดด้วย `O_WRONLY | O_CREAT | O_TRUNC` และกำหนด mode เช่น `0644`

## 4. `setrlimit()` — ตั้งเพดานทรัพยากร

```c
int setrlimit(int resource, const struct rlimit *rlim);
struct rlimit { rlim_t rlim_cur; rlim_t rlim_max; };   // soft, hard
```

- **soft limit** (`rlim_cur`) = เพดานที่มีผลจริง
- **hard limit** (`rlim_max`) = เพดานสูงสุดที่ process ปรับ soft ขึ้นไปได้ (ลดได้ แต่เพิ่มคืนไม่ได้)

| Resource | หน่วย | เกินแล้วเกิดอะไร | Verdict |
|---|---|---|---|
| `RLIMIT_CPU` | **วินาที** | ถึง soft → ได้ `SIGXCPU`; ถึง hard → `SIGKILL` | TLE |
| `RLIMIT_AS` | ไบต์ | `mmap`/`brk` ล้มเหลว (`ENOMEM`) → `malloc` คืน `NULL` | MLE |
| `RLIMIT_FSIZE` | ไบต์ | เขียนไฟล์เกินขนาด → ได้ `SIGXFSZ` | OLE |
| `RLIMIT_STACK` | ไบต์ | stack ล้น → `SIGSEGV` | RE |
| `RLIMIT_NPROC` | จำนวน | `fork` ล้มเหลว (`EAGAIN`) | — |
| `RLIMIT_NOFILE` | จำนวน | `open` ล้มเหลว (`EMFILE`) | — |

**กับดัก**

- `RLIMIT_CPU` เป็น**วินาทีเต็ม** limit 1500 ms ต้องปัดขึ้นเป็น 2 วินาที แล้วเทียบ ms จาก rusage อีกชั้นเอง
- ตั้ง hard ของ `RLIMIT_CPU` สูงกว่า soft สัก 1 วินาที จะได้เห็น `SIGXCPU` (บอกชัดว่าเป็น TLE) ก่อนโดน `SIGKILL`
- `RLIMIT_AS` นับ**หน่วยความจำเสมือนทั้งหมด** รวมโค้ดและไลบรารี มักใหญ่กว่าที่โปรแกรมใช้จริง ให้ตั้งเผื่อไว้ แล้วใช้ `ru_maxrss` ตัดสิน MLE
- `RLIMIT_NPROC` นับ process **ทั้งหมดของ user นั้น** ไม่ใช่แค่ลูกของ judge ถ้ารันด้วย user ปกติที่มี process อื่นอยู่แล้ว ค่า 1 อาจทำให้ judge เองสร้าง process ไม่ได้ ทางแก้ที่ดีกว่าคือให้ seccomp บล็อก `clone` แทน

## 5. `wait4()` — รอลูกจบ พร้อมดึงสถิติ

```c
pid_t wait4(pid_t pid, int *status, int options, struct rusage *ru);
```

- รอจน child `pid` จบ แล้วคืน **สองอย่างในคำสั่งเดียว**: `status` (จบยังไง) และ `rusage` (ใช้ทรัพยากรเท่าไร)
- ถ้าไม่เรียก wait เลย child ที่จบแล้วจะค้างเป็น **zombie**

**อ่าน `status` ด้วย macro**

| Macro | ความหมาย |
|---|---|
| `WIFEXITED(s)` | จบเองปกติ (return / exit) |
| `WEXITSTATUS(s)` | exit code (ใช้ได้เมื่อ `WIFEXITED` เป็นจริง) |
| `WIFSIGNALED(s)` | ถูกฆ่าด้วย signal |
| `WTERMSIG(s)` | เลข signal ที่ฆ่า (ใช้ได้เมื่อ `WIFSIGNALED` เป็นจริง) |

**อ่าน `rusage`**

- `ru_utime` + `ru_stime` = เวลา CPU ฝั่ง user + ฝั่ง kernel เป็น `struct timeval` ต้องแปลงเอง: `tv_sec * 1000 + tv_usec / 1000` = ms
- `ru_maxrss` = หน่วยความจำสูงสุดที่ใช้จริง **บน Linux หน่วยเป็น KB**

**กับดัก**

- ถ้ามี signal (เช่น `SIGALRM`) เข้ามาระหว่างรอ `wait4` อาจคืน `-1` กับ `errno == EINTR` ต้องวนเรียกใหม่ ไม่ใช่ถือว่า error
- เวลา CPU ของ child ที่ถูก `SIGKILL` ก็ยังได้ใน rusage ตามปกติ

## 6. `alarm()` + `sigaction()` + `kill()` — จับเวลาจริง

```c
unsigned alarm(unsigned seconds);          // ตั้งนาฬิกา; alarm(0) = ยกเลิก
int sigaction(int sig, const struct sigaction *act, struct sigaction *old);
int kill(pid_t pid, int sig);
```

- `alarm(n)` → อีก n วินาที kernel ส่ง `SIGALRM` ให้ **process ที่เรียก (parent)**
- ใช้ `sigaction` ตั้ง handler ของ `SIGALRM` ใน handler ให้ `kill` child แล้วตั้ง flag ไว้ว่า "หมดเวลาจริง"
- **ทำไมต้องมี ในเมื่อมี RLIMIT_CPU แล้ว?** โปรแกรมที่ `sleep` หรือรอ input ไม่ใช้ CPU เลย RLIMIT_CPU จึงไม่ทำงาน

**กับดัก**

- ใน signal handler ทำได้แค่ของที่ async-signal-safe เช่น `kill`, `write`, ตั้งตัวแปร `volatile sig_atomic_t` **ห้าม** `printf` หรือ `malloc`
- `kill(pid, sig)` ที่ `pid` **ติดลบ** = ส่งให้ทั้ง process group ถ้า child เรียก `setpgid(0, 0)` ตั้งแต่ต้น parent จะฆ่าทั้งกลุ่มได้ด้วย `kill(-child_pid, SIGKILL)` เผื่อ child แอบสร้างลูกหลานไว้
- ตั้ง `alarm(0)` หลัง `wait4` คืนค่า ไม่งั้นนาฬิกาจะดังไปโดนเคสถัดไป
- ถ้าต้องการความละเอียดต่ำกว่าวินาที ใช้ `setitimer(ITIMER_REAL, …)` แทน alarm ได้

## 7. `prctl(PR_SET_NO_NEW_PRIVS)` — ห้ามยกระดับสิทธิ์

```c
int prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
```

- สัญญากับ kernel ว่า process นี้ (และทุกโปรแกรมที่ execve ต่อจากนี้) จะไม่ได้สิทธิ์เพิ่ม เช่น จากโปรแกรม setuid
- **จำเป็น**ก่อนโหลด seccomp ถ้าไม่ได้รันเป็น root ไม่อย่างนั้นโหลด filter ไม่ผ่าน (`EACCES`)
- ตั้งแล้วถอดไม่ได้ และติดตัวข้าม `fork`/`execve`

## 8. seccomp (ผ่าน libseccomp) — ด่านตรวจ syscall

seccomp คือตัวกรองที่อยู่**ใน kernel** ตรวจทุก syscall ที่ process เรียก แล้วตัดสินตามกฎ: ปล่อยผ่าน, คืน error หรือฆ่าทิ้ง

**ฟังก์ชันหลักของ libseccomp** (link ด้วย `-lseccomp`, include `<seccomp.h>`)

| ฟังก์ชัน | ทำอะไร |
|---|---|
| `seccomp_init(default_action)` | สร้างชุดกฎ และกำหนดว่า syscall ที่ไม่ตรงกฎใดเลยจะโดนอะไร |
| `seccomp_rule_add(ctx, action, syscall, n, ...)` | เพิ่มกฎหนึ่งข้อ ใส่เงื่อนไขของ argument ได้ `n` ตัว |
| `seccomp_syscall_resolve_name("read")` | แปลงชื่อ syscall เป็นเลข |
| `seccomp_load(ctx)` | ติดตั้งกฎเข้า kernel (**ถอดไม่ได้**) |
| `seccomp_release(ctx)` | คืนหน่วยความจำของชุดกฎ (กฎที่โหลดแล้วยังอยู่) |

**Action ที่ควรรู้**

- `SCMP_ACT_ALLOW` → ปล่อยผ่าน
- `SCMP_ACT_KILL_PROCESS` → ฆ่าทั้ง process ด้วย `SIGSYS` (ใช้เป็น default ของเรา)
- `SCMP_ACT_ERRNO(EPERM)` → ไม่ฆ่า แต่ให้ syscall ล้มเหลว (มีประโยชน์ตอน debug)

**Whitelist vs Blacklist**

- **Blacklist** (ห้ามเฉพาะที่รู้ว่าอันตราย) → Linux มี syscall 300 กว่าตัว ลืมตัวเดียวก็รั่ว เช่น ห้าม `unlink` แต่ลืม `unlinkat`
- **Whitelist** (อนุญาตเฉพาะที่จำเป็น) → ไม่รู้จัก = ห้าม **ปลอดภัยกว่า** จึงใช้แบบนี้

**หารายการ whitelist ยังไง**

```bash
gcc -O2 -static -o prog tests/attacks/aplusb_correct.c
echo "1 2" | strace -f ./prog
```

ดูว่าโปรแกรมปกติเรียก syscall อะไรบ้าง แล้วอนุญาตเฉพาะชุดนั้น ทำกับหลาย ๆ โปรแกรม (เช่น ที่ใช้ `malloc` หรือ `scanf` เยอะ ๆ) เผื่อมีตัวที่ตกหล่น

**กับดัก**

- `execve` ต้องอยู่ใน whitelist ไม่งั้นโปรแกรมนักศึกษาเริ่มไม่ได้ แต่ถ้าปล่อยทั้งหมด โปรแกรมก็ `execve("/bin/sh")` ได้ ทางแก้: เพิ่มเงื่อนไขว่า argument ตัวแรกต้องเป็น pointer เดียวกับที่ runner ส่งมา (`SCMP_A0(SCMP_CMP_EQ, …)`)
- ต้อง `seccomp_load` เป็น**ขั้นตอนสุดท้าย**ก่อน execve เพราะหลังจากนั้น syscall ที่ runner ใช้ตั้งค่าเอง (เช่น `setrlimit`, `open`) อาจโดนบล็อก
- เวลาโปรแกรมโดนฆ่าเพราะ seccomp ดู log ได้จาก `sudo dmesg | grep type=1326` จะเห็นเลข syscall ที่โดนบล็อก (บน x86_64 เช่น 41 = `socket`, 87 = `unlink`, 56 = `clone`)
- compile แบบ `-static` จะทำให้ตอนเริ่มโปรแกรมไม่ต้องโหลดไลบรารี (ไม่มี `openat` ไปหา libc) whitelist จึงแคบลงมาก

---

## 9. จาก signal → verdict

ใช้ตารางนี้ตอนเขียนส่วนตัดสินใน `runner.c`

| สิ่งที่เห็นจาก `wait4` | สาเหตุ | Verdict |
|---|---|---|
| flag จาก `SIGALRM` handler ถูกตั้ง | เวลาจริงเกิน | TLE |
| `WTERMSIG == SIGXCPU` หรือเวลา CPU > limit | CPU เกิน | TLE |
| `ru_maxrss` ≥ limit | หน่วยความจำเกิน (แม้จะตายด้วย SIGSEGV) | MLE |
| `WTERMSIG == SIGXFSZ` | output เกิน | OLE |
| `WTERMSIG == SIGSYS` | เรียก syscall ต้องห้าม | SV |
| `WTERMSIG == SIGSEGV / SIGFPE / SIGABRT` | โปรแกรมพัง | RE |
| `WIFEXITED` แต่ exit code ≠ 0 | โปรแกรมคืนค่า error | RE |
| `WIFEXITED` และ exit code = 0 | จบปกติ → ส่งต่อให้ checker | AC / WA |

**ลำดับการเช็กสำคัญ** ให้เช็ก TLE และ MLE **ก่อน** RE เพราะโปรแกรมที่กิน RAM จนเกินมักตายด้วย `SIGSEGV` ถ้าเช็ก RE ก่อนจะตัดสินผิด

---

## 10. ลำดับใน child (สรุปสำหรับ A และ B)

```
fork ─┬─ parent: alarm → wait4 → alarm(0) → ตัดสิน verdict
      └─ child:  setpgid → dup2 (stdin/stdout) → chdir → setrlimit
                 → prctl(NO_NEW_PRIVS) → seccomp_load → execve
```

จำง่าย ๆ: **ต่อสาย → ตั้งเพดาน → ปิดด่าน → เข้าห้อง**
