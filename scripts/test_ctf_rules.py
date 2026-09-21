import sys, subprocess

TARGET = 'c3891fa82c0a842db941d5d4656b35f69ecd45f6'

rules = []
# Digits 0-9
for d in '0123456789':
    rules.append((f'miniCTF{{word{d}}}', lambda w, d=d: b'miniCTF{' + w + d.encode() + b'}\n'))
    rules.append((f'miniCTF{{Word{d}}}', lambda w, d=d: b'miniCTF{' + w.capitalize() + d.encode() + b'}\n'))

# Years 2020-2026
for y in ['2020', '2021', '2022', '2023', '2024', '2025']:
    rules.append((f'miniCTF{{word{y}}}', lambda w, y=y: b'miniCTF{' + w + y.encode() + b'}\n'))
    rules.append((f'miniCTF{{Word{y}}}', lambda w, y=y: b'miniCTF{' + w.capitalize() + y.encode() + b'}\n'))

# Underscore + year
for y in ['2020', '2021', '2022', '2023', '2024', '2025']:
    rules.append((f'miniCTF{{word_{y}}}', lambda w, y=y: b'miniCTF{' + w + b'_' + y.encode() + b'}\n'))

# Common symbols @, #, $, _, -
for s in ['@', '#', '$', '_', '-']:
    rules.append((f'miniCTF{{word{s}}}', lambda w, s=s: b'miniCTF{' + w + s.encode() + b'}\n'))
    rules.append((f'miniCTF{{Word{s}}}', lambda w, s=s: b'miniCTF{' + w.capitalize() + s.encode() + b'}\n'))

print(f'Total rules to test: {len(rules)}')

for name, func in rules:
    print(f'Testing {name}...', flush=True)
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
        # Find which word
        with open('rockyou.txt', 'rb') as f:
            for idx, line in enumerate(f):
                line = line.rstrip(b'\r\n')
                if line:
                    cand = func(line).rstrip(b'\n').decode('latin-1')
                    # verify candidate
                    import hashlib
                    from Crypto.Cipher import AES
                    from Crypto.Util.Padding import pad
                    key = hashlib.sha256(cand.encode('utf-16-le')).digest()
                    c = AES.new(key, AES.MODE_CBC, key[:16]).encrypt(pad(key, 16))
                    if hashlib.sha1(c).digest() == bytes.fromhex(TARGET):
                        print(f'>>> EXACT PASSWORD: {cand} <<<', flush=True)
                        sys.exit(0)
        sys.exit(0)
    else:
        print(f'{name}: NOT FOUND', flush=True)

print('All CTF rules finished.')
