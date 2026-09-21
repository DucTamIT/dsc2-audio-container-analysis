import sys, subprocess

TARGET = 'c3891fa82c0a842db941d5d4656b35f69ecd45f6'

def run_candidates(cand_gen, desc):
    print(f'Starting {desc}...', flush=True)
    p = subprocess.Popen(['ds_gpu_v3.exe', '--hash', TARGET, '--wordlist', '-'],
                         stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    count = 0
    for cand in cand_gen:
        p.stdin.write(cand + b'\n')
        count += 1
    out, err = p.communicate()
    out_str = out.decode('latin-1', 'ignore')
    if 'MATCH' in out_str:
        print(f'*** MATCH FOUND in {desc}! ***\n{out_str}', flush=True)
        sys.exit(0)
    else:
        print(f'{desc} ({count} candidates): NOT FOUND', flush=True)

# 1. Reverse all rockyou words: miniCTF{drow}
def gen_reverse():
    with open('rockyou.txt', 'rb') as f:
        for line in f:
            line = line.rstrip(b'\r\n')
            if line:
                yield b'miniCTF{' + line[::-1] + b'}'

# 2. Duplicate all rockyou words: miniCTF{wordword}
def gen_duplicate():
    with open('rockyou.txt', 'rb') as f:
        for line in f:
            line = line.rstrip(b'\r\n')
            if line and len(line) <= 12: # avoid exceeding max len
                yield b'miniCTF{' + line + line + b'}'

# 3. Prepend digit 0-9 to all rockyou words: miniCTF{0word} .. miniCTF{9word}
def gen_prepend_digit():
    for d in b'0123456789':
        with open('rockyou.txt', 'rb') as f:
            for line in f:
                line = line.rstrip(b'\r\n')
                if line:
                    yield b'miniCTF{' + bytes([d]) + line + b'}'

# 4. Top 200k rockyou + 2 digits (00-99)
def gen_top_2digits():
    top = []
    with open('rockyou.txt', 'rb') as f:
        for i, line in enumerate(f):
            if i >= 200000: break
            line = line.rstrip(b'\r\n')
            if line: top.append(line)
    for w in top:
        for d1 in range(10):
            for d2 in range(10):
                yield b'miniCTF{' + w + f'{d1}{d2}'.encode() + b'}'

# 5. Top 200k Capitalized + 2 digits (00-99)
def gen_top_cap_2digits():
    top = []
    with open('rockyou.txt', 'rb') as f:
        for i, line in enumerate(f):
            if i >= 200000: break
            line = line.rstrip(b'\r\n')
            if line: top.append(line.capitalize())
    for w in top:
        for d1 in range(10):
            for d2 in range(10):
                yield b'miniCTF{' + w + f'{d1}{d2}'.encode() + b'}'

run_candidates(gen_reverse(), 'Jumbo: Reverse words')
run_candidates(gen_duplicate(), 'Jumbo: Duplicate words')
run_candidates(gen_prepend_digit(), 'Jumbo: Prepend digit (0-9)')
run_candidates(gen_top_2digits(), 'Jumbo: Top 200k + 2 digits (00-99)')
run_candidates(gen_top_cap_2digits(), 'Jumbo: Top 200k Cap + 2 digits (00-99)')
print('All Jumbo rule batches finished.')
