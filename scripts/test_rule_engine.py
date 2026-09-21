def parse_hashcat_rule(rule_str):
    """Parse a single hashcat rule line into a list of (op, arg) tuples."""
    ops = []
    i = 0
    s = rule_str.strip()
    while i < len(s):
        if s[i] == ' ' or s[i] == '\t':
            i += 1
            continue
        c = s[i]
        if c in ':lucCtredf{}[]q':
            ops.append((c, ''))
            i += 1
        elif c in '$^@':
            if i + 1 < len(s):
                ops.append((c, s[i+1]))
                i += 2
            else:
                i += 1
        elif c in 's':
            if i + 2 < len(s):
                ops.append((c, (s[i+1], s[i+2])))
                i += 3
            else:
                i += 1
        elif c in 'oi':
            if i + 2 < len(s):
                ops.append((c, (s[i+1], s[i+2])))
                i += 3
            else:
                i += 1
        elif c in 'DT':
            if i + 1 < len(s):
                ops.append((c, s[i+1]))
                i += 2
            else:
                i += 1
        elif c in '<>,.':
            j = i + 1
            while j < len(s) and s[j].isdigit():
                j += 1
            ops.append((c, s[i+1:j]))
            i = j
        elif c in '*+-':
            if i + 2 < len(s):
                ops.append((c, (s[i+1], s[i+2])))
                i += 3
            else:
                i += 1
        elif c in 'Zz':
            if i + 1 < len(s):
                ops.append((c, s[i+1]))
                i += 2
            else:
                i += 1
        else:
            i += 1
    return ops

def apply_parsed_rule(chars, ops):
    s = list(chars)
    for op, arg in ops:
        if op == ':':
            pass
        elif op == 'l':
            s = [c.lower() for c in s]
        elif op == 'u':
            s = [c.upper() for c in s]
        elif op == 'c':
            if s: s = [s[0].upper()] + [c.lower() for c in s[1:]]
        elif op == 'C':
            if s: s = [s[0].lower()] + [c.upper() for c in s[1:]]
        elif op == 't':
            s = [c.lower() if c.isupper() else c.upper() for c in s]
        elif op == 'r':
            s.reverse()
        elif op == 'd':
            s = s + s
        elif op == 'f':
            s = s + s[::-1]
        elif op == '{':
            if s: s = s[1:] + s[:1]
        elif op == '}':
            if s: s = s[-1:] + s[:-1]
        elif op == '[':
            if s: s.pop(0)
        elif op == ']':
            if s: s.pop()
        elif op == '$':
            s.append(arg)
        elif op == '^':
            s.insert(0, arg)
        elif op == 's':
            old, new = arg
            s = [new if c == old else c for c in s]
        elif op == '@':
            s = [c for c in s if c != arg]
        elif op == 'o':
            idx_ch, ch = arg
            idx = int(idx_ch, 16) if idx_ch in '0123456789abcdefABCDEF' else -1
            if 0 <= idx < len(s): s[idx] = ch
        elif op == 'i':
            idx_ch, ch = arg
            idx = int(idx_ch, 16) if idx_ch in '0123456789abcdefABCDEF' else -1
            if 0 <= idx <= len(s): s.insert(idx, ch)
        elif op == 'D':
            idx = int(arg, 16) if arg in '0123456789abcdefABCDEF' else -1
            if 0 <= idx < len(s): s.pop(idx)
        elif op == 'T':
            idx = int(arg, 16) if arg in '0123456789abcdefABCDEF' else -1
            if 0 <= idx < len(s):
                s[idx] = s[idx].lower() if s[idx].isupper() else s[idx].upper()
        elif op in '<,':
            try:
                lim = int(arg)
                if len(s) >= lim: return None
            except: pass
        elif op in '>.':
            try:
                lim = int(arg)
                if len(s) <= lim: return None
            except: pass
    return ''.join(s)

if __name__ == '__main__':
    for r in [':', 'c $1', 'c $1 $2 $3', 'r', 'sa@', 'o0P', 'c $!']:
        ops = parse_hashcat_rule(r)
        res = apply_parsed_rule('password', ops)
        print(f'{r:15s} -> {res}')
