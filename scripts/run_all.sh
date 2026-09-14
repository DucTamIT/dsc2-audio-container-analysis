#!/bin/bash
# Prioritised search order. Every step logs to logs/<name>.log so results survive.
H=c3891fa82c0a842db941d5d4656b35f69ecd45f6
B=16777216
mkdir -p logs
run() {
  name="$1"; shift
  echo "=== $name : $* ===" | tee -a logs/master.log
  ./ds_gpu --hash $H "$@" --batch $B 2>&1 | tail -3 | tee "logs/$name.log"
  rc=${PIPESTATUS[0]}
  if [ "$rc" = "0" ]; then echo ">>> FOUND in $name"; exit 0; fi
}
# 1) Vietnamese passphrases (most likely if the password is a phrase)
run vn2_nosep   --wordfile vie_words.txt  --wordfile vie_words.txt
run vn2_space   --wordfile vie_words.txt  --wordfile vie_words.txt --sep ' '
run vn3_space   --wordfile vie_common.txt --wordfile vie_common.txt --wordfile vie_common.txt --sep ' '
run vn3_nosep   --wordfile vie_common.txt --wordfile vie_common.txt --wordfile vie_common.txt
run vn2_num     --wordfile vie_common.txt --wordfile vie_common.txt --wordfile nums.txt --sep '_'
# 2) all printable ASCII, length 6
run printable6  --printable 6
run printable7  --printable 7
# 3) broader alnum
run alnum7      --mask abcdefghijklmnopqrstuvwxyz0123456789 7
run lower8      --mask abcdefghijklmnopqrstuvwxyz 8
run alnum8      --mask abcdefghijklmnopqrstuvwxyz0123456789 8
# 4) braces / mixed case
run brace6      --mask 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_{}' 6
run brace7      --mask 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_{}' 7
echo "ALL DONE - nothing found" | tee -a logs/master.log
