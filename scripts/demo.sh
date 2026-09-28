#!/usr/bin/env bash
# เดโมสำหรับนำเสนอ — กด Enter เพื่อไปขั้นถัดไป, Ctrl+C เพื่อออก
# ใช้:  bash scripts/demo.sh
# แนะนำ: เปิด htop ไว้อีกหน้าต่าง ให้ผู้ชมเห็นว่าเครื่องไม่ค้าง
set -u
cd "$(dirname "$0")/.."

JUDGE=./judge
PROBLEM=tests/problems/aplusb
ATK=tests/attacks
CANARY=/tmp/judge_canary.txt
PWNED=/tmp/judge_pwned
TMPBIN=$(mktemp -d /tmp/judge-demo.XXXXXX)
trap 'rm -rf "$TMPBIN"' EXIT

B=$'\e[1m'; DIM=$'\e[2m'; R=$'\e[31m'; G=$'\e[32m'; Y=$'\e[33m'; C=$'\e[36m'; N=$'\e[0m'

title() { printf '\n%s━━━ %s ━━━%s\n' "$B$C" "$1" "$N"; }
say()   { printf '%s%s%s\n' "$DIM" "$1" "$N"; }
pause() { printf '\n%s[Enter] %s%s' "$Y" "${1:-ต่อไป}" "$N"; read -r _; }
show()  { printf '%s$ %s%s\n' "$G" "$*" "$N"; "$@"; }
judge() { show "$JUDGE" --src "$ATK/$1" --tests "$PROBLEM" < /dev/null; }
check_canary() {
    if [ -f "$CANARY" ]; then printf '%s✓ %s ยังอยู่%s\n' "$G" "$CANARY" "$N"
    else printf '%s✗ %s ถูกลบไปแล้ว!%s\n' "$R" "$CANARY" "$N"; fi
}
check_pwned() {
    if [ -e "$PWNED" ]; then printf '%s✗ %s ถูกสร้างขึ้น — shell ทำงานได้!%s\n' "$R" "$PWNED" "$N"
    else printf '%s✓ %s ไม่ถูกสร้าง%s\n' "$G" "$PWNED" "$N"; fi
}

[ -x "$JUDGE" ] || { echo "ไม่พบ $JUDGE — รัน make ก่อน"; exit 1; }

clear
printf '%sSecure Code Judge Sandbox%s\n' "$B" "$N"
say "โจทย์: จะรันโค้ดที่ไม่ไว้ใจได้อย่างไร โดยไม่ให้เครื่องพัง"
pause "เริ่ม"

# ------------------------------------------------------------------
title "1. โปรแกรมปกติ"
say "โค้ดถูก → AC, โค้ดผิด → WA"
judge aplusb_correct.c
pause
judge aplusb_wrong.c
pause "ไปดูโค้ดที่กินทรัพยากร"

# ------------------------------------------------------------------
title "2. กินทรัพยากร — setrlimit + alarm"
say "วนลูปไม่จบ → RLIMIT_CPU ส่ง SIGXCPU"
judge infinite_loop.c
pause
say "sleep(100) ไม่ใช้ CPU → RLIMIT_CPU ไม่ช่วย ต้องใช้ alarm ของ parent"
judge sleep_forever.c
pause
say "malloc ไม่หยุด → RLIMIT_AS + ru_maxrss"
judge memory_hog.c
pause
say "พิมพ์ไม่หยุด → RLIMIT_FSIZE ส่ง SIGXFSZ"
judge output_flood.c
pause "ไปดูโค้ดโจมตี (ก่อน / หลัง)"

# ------------------------------------------------------------------
title "3. ลบไฟล์ — ก่อน / หลัง sandbox"
echo "canary" > "$CANARY"
say "ก่อน: compile แล้วรันตรง ๆ ไม่มีกรง"
show gcc -O2 -o "$TMPBIN/delete_file" "$ATK/delete_file.c"
show "$TMPBIN/delete_file"
check_canary
pause "ลองใหม่ผ่าน judge"
echo "canary" > "$CANARY"
say "หลัง: รันผ่าน judge"
judge delete_file.c
check_canary
pause

title "4. เรียก shell — ก่อน / หลัง sandbox"
rm -f "$PWNED"
say "ก่อน: รันตรง ๆ (ตัวอย่างนี้ใช้ touch แทน rm -rf โดยตั้งใจ)"
show gcc -O2 -o "$TMPBIN/shell" "$ATK/system_call_shell.c"
show "$TMPBIN/shell"
check_pwned
pause "ลองใหม่ผ่าน judge"
rm -f "$PWNED"
judge system_call_shell.c
check_pwned
pause

title "5. ต่อเน็ต และ fork bomb — ผ่าน judge เท่านั้น"
judge open_socket.c
pause
say "ดู htop: จำนวน process ต้องไม่เพิ่ม"
judge fork_bomb.c
pause "ดูหลักฐานจาก kernel"

# ------------------------------------------------------------------
title "6. หลักฐานจาก kernel (seccomp audit log)"
say "เลข syscall บน x86_64: 41 = socket, 56 = clone, 87 = unlink, 435 = clone3"
DMESG="dmesg"
dmesg >/dev/null 2>&1 || DMESG="sudo dmesg"
printf '%s$ %s | grep type=1326 | tail -n 4%s\n' "$G" "$DMESG" "$N"
$DMESG | grep 'type=1326' | tail -n 4
[ "${PIPESTATUS[1]}" -eq 0 ] || say "(ไม่พบ log ของ seccomp)"
pause "รันทดสอบทั้งหมด"

# ------------------------------------------------------------------
title "7. ทดสอบทั้งหมด"
show bash scripts/run_all_tests.sh
rm -f "$CANARY" "$PWNED"

printf '\n%sจบเดโม — เครื่องยังทำงานปกติ%s\n' "$B$G" "$N"
