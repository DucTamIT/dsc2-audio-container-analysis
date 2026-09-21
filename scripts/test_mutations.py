import sys, subprocess

TARGET = 'c3891fa82c0a842db941d5d4656b35f69ecd45f6'

# 1. Common symbols (excluding ! which was already tested)
symbols = ['@', '#', '$', '%', '&', '*', '?', '.', '_', '-']
for sym in symbols:
    p = subprocess.Popen(['ds_gpu_v3.exe', '--hash', TARGET, '--wordlist', '-'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    with open('rockyou.txt', 'rb') as f:
        for line in f:
            line = line.rstrip(b'\r\n')
            if line:
                p.stdin.write(line + sym.encode() + b'\n')
    out, err = p.communicate()
    out_str = out.decode('latin-1', 'ignore')
    if 'MATCH' in out_str:
        print(f'*** MATCH FOUND with symbol {sym}:', out_str)
        sys.exit(0)
    print(f'Symbol {sym}: not found', flush=True)

# 2. Top 100k rockyou + 00..99
print('Reading top 100k rockyou...')
top_words = []
with open('rockyou.txt', 'rb') as f:
    for i, line in enumerate(f):
        if i >= 100000: break
        line = line.rstrip(b'\r\n')
        if line: top_words.append(line)

print(f'Testing top {len(top_words)} words with 2-digit suffixes (00..99)...')
p = subprocess.Popen(['ds_gpu_v3.exe', '--hash', TARGET, '--wordlist', '-'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
for w in top_words:
    for d1 in range(10):
        for d2 in range(10):
            p.stdin.write(w + f'{d1}{d2}\n'.encode())
out, err = p.communicate()
out_str = out.decode('latin-1', 'ignore')
if 'MATCH' in out_str:
    print('*** MATCH FOUND with 2 digits:', out_str)
    sys.exit(0)
print('Top 100k + 2-digit suffixes: not found')
