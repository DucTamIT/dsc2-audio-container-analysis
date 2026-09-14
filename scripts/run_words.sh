#!/bin/bash
# Wait for the braces run to release the GPU, then search Vietnamese passphrases.
H=c3891fa82c0a842db941d5d4656b35f69ecd45f6
B=16777216
while pgrep -f run_braces.sh > /dev/null; do sleep 30; done
run() { ./ds_gpu "$@"; rc=$?; if [ $rc -eq 0 ]; then echo ">>> FOUND"; exit 0; fi; }
echo "== 2 words from the full 77k Vietnamese dictionary, no separator =="
run --hash $H --wordfile vie_words.txt --wordfile vie_words.txt --batch $B
echo "== same, space separator =="
run --hash $H --wordfile vie_words.txt --wordfile vie_words.txt --sep ' ' --batch $B
echo "== 3 words from the 3000 most common, space separator =="
run --hash $H --wordfile vie_common.txt --wordfile vie_common.txt --wordfile vie_common.txt --sep ' ' --batch $B
echo "== 3 words, no separator =="
run --hash $H --wordfile vie_common.txt --wordfile vie_common.txt --wordfile vie_common.txt --batch $B
echo "== 2 words + year/digits suffix =="
printf '2026\n123\n1\n2024\n2025\n' > nums.txt
for SEP in '' ' ' '_'; do
  run --hash $H --wordfile vie_common.txt --wordfile vie_common.txt --wordfile nums.txt --sep "$SEP" --batch $B
done
echo "ALL WORD RUNS DONE - nothing found"
