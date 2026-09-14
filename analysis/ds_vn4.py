import hashlib, itertools, unicodedata, multiprocessing as mp
from Crypto.Cipher import AES
from Crypto.Util.Padding import pad
T=bytes.fromhex("c3891fa82c0a842db941d5d4656b35f69ecd45f6")
V=("hát bé thế chả chẳng nghe thấy gì to lên nhỏ vặn mở bật tăng âm lượng tiếng giọng vang loa tai "
"điếc quá rồi đi nào thì mà cứ hơn nữa cho tôi mình em anh chị ạ nhé nha với đấy đó kia thật hay dở "
"được không có phải rất hơi lắm ghê trời ơi chúa ơi mẹ bà ông ca sĩ bài hát nhạc micro loa phóng thanh").split()
V=sorted(set(V))
def v(pw):
    key=hashlib.sha256(pw.encode("utf-16-le")).digest()
    if hashlib.sha1(AES.new(key,AES.MODE_CBC,key[:16]).encrypt(pad(key,16))).digest()==T: return "DSC2"
    b=bytearray(32); e=pw.encode("utf-8"); n=min(len(e),32); b[:n]=e[:n]
    if hashlib.sha1(bytes(b)).digest()==T: return "DSCF"
    return None
def work(chunk):
    for r in range(1,5):
        for combo in itertools.product(V, repeat=r-1):
            words=(chunk,)+combo
            for sep in ("", " ", "_"):
                s=sep.join(words)
                for c in (s, s.capitalize()):
                    res=v(c)
                    if res: return (res, c)
    return None
if __name__=="__main__":
    ctx=mp.get_context("fork")
    print("vocab", len(V), flush=True)
    with ctx.Pool(8) as p:
        for r in p.imap_unordered(work, V):
            if r: print("FOUND:", r, flush=True)
    print("done", flush=True)
