import sys, subprocess
from test_rule_engine import parse_hashcat_rule, apply_parsed_rule

TARGET = 'c3891fa82c0a842db941d5d4656b35f69ecd45f6'

# Load rules
print('Loading OneRuleToRuleThemStill.rule...', flush=True)
rules = []
with open('OneRuleToRuleThemStill.rule', 'r', encoding='latin-1') as f:
    for line in f:
        line = line.strip()
        if line and not line.startswith('#'):
            ops = parse_hashcat_rule(line)
            if ops:
                rules.append(ops)
print(f'Loaded {len(rules)} rules.', flush=True)

# Phase 1: Challenge keywords
keywords = [
    'tokuda', 'Tokuda', 'TOKUDA',
    'lance', 'Lance', 'LANCE',
    'lancetokuda', 'LanceTokuda',
    'rockyou', 'RockYou', 'ROCKYOU',
    'deepsound', 'DeepSound', 'DEEPSOUND',
    'conchimnon', 'ConChimNon', 'chimnon',
    'thevoice', 'TheVoice', 'voice',
    'xuanmai', 'XuanMai', 'bexuanmai',
    'hatbe', 'hatbethe', 'changhethaygi',
]

print(f'Phase 1: Testing {len(keywords)} challenge keywords x {len(rules)} rules...', flush=True)
cands = set()
for kw in keywords:
    for r in rules:
        res = apply_parsed_rule(kw, r)
        if res and 1 <= len(res) <= 30:
            cands.add(res)

print(f'Phase 1: Generated {len(cands)} unique mutated candidates. Streaming to GPU...', flush=True)
p = subprocess.Popen(['ds_gpu_v3.exe', '--hash', TARGET, '--wordlist', '-'],
                     stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
for c in cands:
    p.stdin.write(f'miniCTF{{{c}}}\n'.encode('latin-1', 'ignore'))
out, err = p.communicate()
out_str = out.decode('latin-1', 'ignore')
if 'MATCH' in out_str:
    print('*** MATCH FOUND in Phase 1! ***\n', out_str)
    sys.exit(0)
print('Phase 1: NOT FOUND\n', flush=True)

# Phase 2: Top RockYou words
TOP_COUNT = 10000
print(f'Phase 2: Testing top {TOP_COUNT} rockyou words x {len(rules)} rules...', flush=True)

top_words = []
with open('rockyou.txt', 'rb') as f:
    for i, line in enumerate(f):
        if i >= TOP_COUNT: break
        line = line.rstrip(b'\r\n')
        if line:
            try:
                top_words.append(line.decode('latin-1'))
            except:
                pass

print(f'Read {len(top_words)} words. Streaming to GPU in batches of 1000 words...', flush=True)

batch_size = 1000
for b_idx in range(0, len(top_words), batch_size):
    batch = top_words[b_idx:b_idx+batch_size]
    batch_cands = set()
    for w in batch:
        for r in rules:
            res = apply_parsed_rule(w, r)
            if res and 1 <= len(res) <= 30:
                batch_cands.add(res)
    
    p = subprocess.Popen(['ds_gpu_v3.exe', '--hash', TARGET, '--wordlist', '-'],
                         stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    for c in batch_cands:
        p.stdin.write(f'miniCTF{{{c}}}\n'.encode('latin-1', 'ignore'))
    out, err = p.communicate()
    out_str = out.decode('latin-1', 'ignore')
    if 'MATCH' in out_str:
        print(f'*** MATCH FOUND in batch {b_idx}-{b_idx+len(batch)}! ***\n', out_str)
        sys.exit(0)
    print(f'Words {b_idx}..{b_idx+len(batch)} ({len(batch_cands)} cands): NOT FOUND', flush=True)

print('All OneRule-wrapped tests completed.')
