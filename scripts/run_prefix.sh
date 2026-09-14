#!/bin/bash
H=c3891fa82c0a842db941d5d4656b35f69ecd45f6
INNER='abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_'
B=16777216
while pgrep -f run_words.sh > /dev/null; do sleep 30; done
run() { ./ds_gpu "$@"; rc=$?; if [ $rc -eq 0 ]; then echo ">>> FOUND"; exit 0; fi; }
for P in Pragyan pragyan "0xFun" 0xfun EHAX ehax ApoorvCTF apoorv UTCTF utctf BSidesSF bsidessf INShAck inshack BaltCTF BYPASS bypass XMAS SecurityFest BackdoorCTF thevoice TheVoice; do
  for L in 1 2 3 4 5 6 7; do
    run --hash $H --prefix "${P}{" --suffix '}' --mask "$INNER" $L --batch $B
  done
  run --hash $H --prefix "${P}_" --mask "$INNER" 5 --batch $B
  run --hash $H --prefix "$P" --mask "$INNER" 6 --batch $B
done
echo "PREFIX RUNS DONE - nothing found"
