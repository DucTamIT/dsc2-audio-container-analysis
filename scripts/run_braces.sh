#!/bin/bash
H=c3891fa82c0a842db941d5d4656b35f69ecd45f6
CS='abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_{}'
INNER='abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_'
B=16777216
run() { ./ds_gpu "$@"; rc=$?; if [ $rc -eq 0 ]; then echo ">>> FOUND (see above)"; exit 0; fi; }
echo "== stage 1: full 65-char masks, lengths 4..6 =="
for L in 4 5 6; do run --hash $H --mask "$CS" $L --batch $B; done
echo "== stage 2: PREFIX{ inner } =="
for P in miniCTF MiniCTF flag FLAG GZCTF ISITDTU VNT ASCIS WhiteHat HCMUS UIT PTIT FPT CTF2026 miniCTF2026; do
  for L in 1 2 3 4 5 6; do run --hash $H --prefix "${P}{" --suffix '}' --mask "$INNER" $L --batch $B; done
done
echo "== stage 3: PREFIX<inner> without braces =="
for P in miniCTF flag CTF; do
  for L in 4 5 6 7; do run --hash $H --prefix "$P" --mask "$INNER" $L --batch $B; done
done
echo "ALL DONE - nothing found"
