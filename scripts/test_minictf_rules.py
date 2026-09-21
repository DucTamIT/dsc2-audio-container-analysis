import sys, subprocess

TARGET = 'c3891fa82c0a842db941d5d4656b35f69ecd45f6'

patterns = [
    ('miniCTF{word2026}', lambda w: b'miniCTF{' + w + b'2026}\n'),
    ('miniCTF{Word2026}', lambda w: b'miniCTF{' + w.capitalize() + b'2026}\n'),
    ('miniCTF{WORD2026}', lambda w: b'miniCTF{' + w.upper() + b'2026}\n'),
    ('miniCTF{word_2026}', lambda w: b'miniCTF{' + w + b'_2026}\n'),
    ('miniCTF{Word_2026}', lambda w: b'miniCTF{' + w.capitalize() + b'_2026}\n'),
    ('miniCTF{word!}', lambda w: b'miniCTF{' + w + b'!}\n'),
    ('miniCTF{Word!}', lambda w: b'miniCTF{' + w.capitalize() + b'!}\n'),
    ('miniCTF{word123}', lambda w: b'miniCTF{' + w + b'123}\n'),
    ('miniCTF{Word123}', lambda w: b'miniCTF{' + w.capitalize() + b'123}\n'),
    ('minictf{word2026}', lambda w: b'minictf{' + w + b'2026}\n'),
]

for name, func in patterns:
    print(f'Testing {name}...')
    p = subprocess.Popen(['ds_gpu_v3.exe', '--hash', TARGET, '--wordlist', '-'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    with open('rockyou.txt', 'rb') as f:
        for line in f:
            line = line.rstrip(b'\r\n')
            if line:
                p.stdin.write(func(line))
    out, err = p.communicate()
    out_str = out.decode('latin-1', 'ignore')
    if 'MATCH' in out_str:
        print(f'*** MATCH FOUND for {name}:', out_str)
        sys.exit(0)
    print(f'{name}: NOT FOUND', flush=True)

print('All miniCTF rules tested.')
