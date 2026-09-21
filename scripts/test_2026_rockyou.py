import sys, subprocess, hashlib
from Crypto.Cipher import AES
from Crypto.Util.Padding import pad

TARGET = 'c3891fa82c0a842db941d5d4656b35f69ecd45f6'
TARGET_BYTES = bytes.fromhex(TARGET)

def verify_cand(cand):
    key = hashlib.sha256(cand.encode('utf-16-le')).digest()
    c = AES.new(key, AES.MODE_CBC, key[:16]).encrypt(pad(key, 16))
    return hashlib.sha1(c).digest() == TARGET_BYTES

patterns = [
    # Most likely: CTF flag format wrapping rockyou word + 2026
    ("miniCTF{word2026}", lambda w: b'miniCTF{' + w + b'2026}\n'),
    ("miniCTF{word_2026}", lambda w: b'miniCTF{' + w + b'_2026}\n'),
    ("miniCTF{Word2026}", lambda w: b'miniCTF{' + w.capitalize() + b'2026}\n'),
    ("miniCTF{Word_2026}", lambda w: b'miniCTF{' + w.capitalize() + b'_2026}\n'),
    ("miniCTF{WORD2026}", lambda w: b'miniCTF{' + w.upper() + b'2026}\n'),
    ("miniCTF{WORD_2026}", lambda w: b'miniCTF{' + w.upper() + b'_2026}\n'),
    
    # Lowercase & uppercase flag wrapper
    ("minictf{word2026}", lambda w: b'minictf{' + w + b'2026}\n'),
    ("minictf{word_2026}", lambda w: b'minictf{' + w + b'_2026}\n'),
    ("MINICTF{word2026}", lambda w: b'MINICTF{' + w + b'2026}\n'),
    ("MINICTF{WORD2026}", lambda w: b'MINICTF{' + w.upper() + b'2026}\n'),
    ("MINICTF{word_2026}", lambda w: b'MINICTF{' + w + b'_2026}\n'),
    ("MINICTF{WORD_2026}", lambda w: b'MINICTF{' + w.upper() + b'_2026}\n'),

    # Direct passwords without miniCTF
    ("word2026", lambda w: w + b'2026\n'),
    ("word_2026", lambda w: w + b'_2026\n'),
    ("Word2026", lambda w: w.capitalize() + b'2026\n'),
    ("Word_2026", lambda w: w.capitalize() + b'_2026\n'),
    ("WORD2026", lambda w: w.upper() + b'2026\n'),
    ("WORD_2026", lambda w: w.upper() + b'_2026\n'),

    # Trailing exclamation marks / symbols
    ("miniCTF{word2026!}", lambda w: b'miniCTF{' + w + b'2026!}\n'),
    ("miniCTF{Word2026!}", lambda w: b'miniCTF{' + w.capitalize() + b'2026!}\n'),
    ("miniCTF{word_2026!}", lambda w: b'miniCTF{' + w + b'_2026!}\n'),
    ("miniCTF{word2026?}", lambda w: b'miniCTF{' + w + b'2026?}\n'),
    ("miniCTF{word2026.}", lambda w: b'miniCTF{' + w + b'2026.}\n'),
    ("miniCTF{word.2026}", lambda w: b'miniCTF{' + w + b'.2026}\n'),
]

for name, func in patterns:
    print(f"Testing pattern '{name}' against rockyou.txt...", flush=True)
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
        print(f"\n==========================================")
        print(f"*** MATCH FOUND for pattern {name}! ***")
        print(out_str)
        print("Finding exact matching line...")
        with open('rockyou.txt', 'rb') as f:
            for idx, line in enumerate(f):
                line = line.rstrip(b'\r\n')
                if line:
                    cand = func(line).rstrip(b'\n').decode('latin-1', 'ignore')
                    if verify_cand(cand):
                        print(f"!!! CRACKED PASSWORD: {cand} !!!")
                        print(f"Line index in rockyou: {idx}, base word: {line}")
                        print(f"==========================================")
                        sys.exit(0)
        print("Exact search loop finished.")
        sys.exit(0)
    else:
        print(f"{name}: NOT FOUND", flush=True)

print("All 2026 rockyou patterns finished.")
