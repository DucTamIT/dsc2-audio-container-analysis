import numpy as np
raw=open("Con chim non.wav","rb").read()
a=np.frombuffer(raw[46:],dtype="<i2").reshape(-1,2).astype(np.float32)
sr=44100
MONO=a.mean(1)
MORSE={'.-':'A','-...':'B','-.-.':'C','-..':'D','.':'E','..-.':'F','--.':'G','....':'H','..':'I',
'.---':'J','-.-':'K','.-..':'L','--':'M','-.':'N','---':'O','.--.':'P','--.-':'Q','.-.':'R',
'...':'S','-':'T','..-':'U','...-':'V','.--':'W','-..-':'X','-.--':'Y','--..':'Z',
'-----':'0','.----':'1','..---':'2','...--':'3','....-':'4','.....':'5','-....':'6','--...':'7','---..':'8','----.':'9'}

def stft(x, n=2048, hop=512):
    win=np.hanning(n).astype(np.float32)
    nf=1+(len(x)-n)//hop
    idx=np.arange(n)[None,:]+hop*np.arange(nf)[:,None]
    return np.abs(np.fft.rfft(x[idx]*win,axis=1)), sr/n

def morse_from_env(env, fps, thr_k):
    env=env-env.min()
    if env.max()<=0: return ""
    e=env/env.max()
    thr=np.percentile(e, 100*thr_k)
    on=e>thr
    # runs
    runs=[]; cur=on[0]; ln=1
    for v in on[1:]:
        if v==cur: ln+=1
        else: runs.append((cur,ln)); cur=v; ln=1
    runs.append((cur,ln))
    ons=[l for s,l in runs if s]
    if len(ons)<4: return ""
    unit=min(ons)
    out=[]; sym=""
    for s,l in runs:
        if s:
            sym += "." if l < 2.5*unit else "-"
        else:
            if l < 2.5*unit: pass
            elif l < 6*unit:
                out.append(MORSE.get(sym,'?')); sym=""
            else:
                if sym: out.append(MORSE.get(sym,'?')); sym=""
                out.append(" ")
    if sym: out.append(MORSE.get(sym,'?'))
    return "".join(out)

S,fb=stft(MONO)
best=[]
for f in range(300, 8001, 100):
    b=int(round(f/fb)); b=min(b, S.shape[1]-1)
    env=S[:,b]
    for k in (0.5,0.65,0.8):
        txt=morse_from_env(env, sr/512, k)
        if len(txt)>=6:
            letters=sum(c.isalpha() or c.isdigit() for c in txt)
            if letters>=6 and letters/len(txt)>0.8:
                best.append((letters, f, k, txt[:70]))
best.sort(reverse=True)
print("candidate Morse decodes (letters, freq, thr, text):")
for b in best[:12]: print("  ", b)
print("total candidates:", len(best))
