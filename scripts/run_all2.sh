#!/bin/bash
H=c3891fa82c0a842db941d5d4656b35f69ecd45f6
B=16777216
mkdir -p logs
run() {
  name="$1"; shift
  echo "=== $name : $* ===" | tee -a logs/master2.log
  ./ds_gpu --hash $H "$@" --batch $B 2>&1 | tail -3 | tee "logs/$name.log"
  rc=${PIPESTATUS[0]}
  if [ "$rc" = "0" ]; then echo ">>> FOUND in $name"; cp logs/$name.log logs/FOUND_$name.log; exit 0; fi
}
run vn2_num      --wordfile vie_common.txt --wordfile vie_common.txt --wordfile nums.txt --sep '_'
run lower7       --mask abcdefghijklmnopqrstuvwxyz 7          # (already done, cheap re-check)
run alnum7       --mask abcdefghijklmnopqrstuvwxyz0123456789 7
run brace6       --mask 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_{}' 6
run lower8       --mask abcdefghijklmnopqrstuvwxyz 8
run alnum8       --mask abcdefghijklmnopqrstuvwxyz0123456789 8
echo "ALL DONE (feasible spaces) - nothing found" | tee -a logs/master2.log
