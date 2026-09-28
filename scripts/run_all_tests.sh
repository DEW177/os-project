#!/usr/bin/env bash
# รันทุกไฟล์ใน tests/attacks/ แล้วเทียบ verdict กับ tests/attacks/expected.txt
# ใช้:  make test   หรือ   bash scripts/run_all_tests.sh
set -u
cd "$(dirname "$0")/.."

JUDGE=./judge
PROBLEM=tests/problems/aplusb
CANARY=/tmp/judge_canary.txt
PWNED=/tmp/judge_pwned

[ -x "$JUDGE" ] || { echo "ไม่พบ $JUDGE — รัน make ก่อน"; exit 1; }

pass=0; fail=0
while read -r file expected; do
    [[ -z "${file:-}" || "$file" == \#* ]] && continue

    echo "canary" > "$CANARY"
    rm -f "$PWNED"

    out=$("$JUDGE" --src "tests/attacks/$file" --tests "$PROBLEM" \
                   --time 1000 --mem 256 --output 1024 < /dev/null 2>&1)
    got=$(printf '%s\n' "$out" | sed -n 's/^RESULT: \([A-Z]*\).*/\1/p' | tail -n 1)

    note=""
    [ -f "$CANARY" ] || note+=" [canary ถูกลบ!]"
    [ -e "$PWNED"  ] && note+=" [shell ถูกเรียกได้!]"

    if [[ "$got" == "$expected" && -z "$note" ]]; then
        printf "PASS  %-22s %-4s\n" "$file" "$got"
        pass=$((pass + 1))
    else
        printf "FAIL  %-22s expected=%-4s got=%-4s%s\n" "$file" "$expected" "${got:-?}" "$note"
        fail=$((fail + 1))
    fi
done < tests/attacks/expected.txt

rm -f "$CANARY" "$PWNED"

echo
echo "Passed $pass / $((pass + fail))"
if ps -eo stat= | grep -q '^Z'; then
    echo "WARNING: พบ zombie process — ตรวจ wait4 ใน runner.c"
fi
[ "$fail" -eq 0 ]
