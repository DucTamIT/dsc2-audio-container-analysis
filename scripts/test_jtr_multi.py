import sys, subprocess

TARGET = 'c3891fa82c0a842db941d5d4656b35f69ecd45f6'

rules = [
    ('miniCTF{word1234}', lambda w: b'miniCTF{' + w + b'1234}\n'),
    ('miniCTF{Word1234}', lambda w: b'miniCTF{' + w.capitalize() + b'1234}\n'),
    ('miniCTF{word12345}', lambda w: b'miniCTF{' + w + b'12345}\n'),
    ('miniCTF{Word12345}', lambda w: b'miniCTF{' + w.capitalize() + b'12345}\n'),
    ('miniCTF{word123456}', lambda w: b'miniCTF{' + w + b'123456}\n'),
    ('miniCTF{Word123456}', lambda w: b'miniCTF{' + w.capitalize() + b'123456}\n'),
    ('miniCTF{word007}', lambda w: b'miniCTF{' + w + b'007}\n'),
]

# Triple repeating digits 000, 111, ..., 999
for d in '0123456789':
    rules.append((f'miniCTF{{word{d*3}}}', lambda w, d=d: b'miniCTF{' + w + (d*3).encode() + b'}\n'))
    rules.append((f'miniCTF{{Word{d*3}}}', lambda w, d=d: b'miniCTF{' + w.capitalize() + (d*3).encode() + b'}\n'))

# Years 2000-2019
for y in range(2000, 2020):
    ys = str(y).encode()
    rules.append((f'miniCTF{{word{y}}}', lambda w, ys=ys: b'miniCTF{' + w + ys + b'}\n'))
    rules.append((f'miniCTF{{Word{y}}}', lambda w, ys=ys: b'miniCTF{' + w.capitalize() + ys + b'}\n'))

# Most common birth years 1980-1999
for y in range(1980, 2000):
    ys = str(y).encode()
    rules.append((f'miniCTF{{word{y}}}', lambda w, ys=ys: b'miniCTF{' + w + ys + b'}\n'))
    rules.append((f'miniCTF{{Word{y}}}', lambda w, ys=ys: b'miniCTF{' + w.capitalize() + ys + b'}\n'))

print(f'Total JtR multi-digit/year rules to test: {len(rules)}')

for name, func in rules:
    print(f'Starting {name}...', flush=True)
    p = subprocess.Popen(['ds_gpu_v3.exe', '--hash', TARGET, '--wordlist', '-'],
                         stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    with open('rockyou.txt', 'rb') as f:
        for line in f:
            line = line.rstrip(b'\r\n')
            if line:
                try:
                    p.stdin.write(func(line))
                except Exception:
                    pass
    out, err = p.communicate()
    out_str = out.decode('latin-1', 'ignore')
    if 'MATCH' in out_str:
        print(f'*** MATCH FOUND for {name}! ***\n{out_str}', flush=True)
        sys.exit(0)
    else:
        print(f'{name}: NOT FOUND', flush=True)

print('All JtR multi-digit/year rules finished.')
