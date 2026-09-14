import unicodedata, hashlib, itertools
from Crypto.Cipher import AES
from Crypto.Util.Padding import pad
T=bytes.fromhex("c3891fa82c0a842db941d5d4656b35f69ecd45f6")
def v(pw):
    key=hashlib.sha256(pw.encode("utf-16-le")).digest()
    if hashlib.sha1(AES.new(key,AES.MODE_CBC,key[:16]).encrypt(pad(key,16))).digest()==T: return "DSC2"
    b=bytearray(32); e=pw.encode("utf-8"); n=min(len(e),32); b[:n]=e[:n]
    if hashlib.sha1(bytes(b)).digest()==T: return "DSCF"
    return None
# tone marks and modified letters
TONE={'́':'s','̀':'f','̉':'r','̃':'x','̣':'j'}            # acute grave hook tilde dot  (TELEX)
TONE_VNI={'́':'1','̀':'2','̉':'3','̃':'4','̣':'5'}
TONE_VIQR={'́':"'",'̀':'`','̉':'?','̃':'~','̣':'.'}
BASE_TELEX={'a':'a','ă':'aw','â':'aa','e':'e','ê':'ee','i':'i','o':'o','ô':'oo','ơ':'ow','u':'u','ư':'uw','y':'y','d':'d'}
def encode(s, tone_map, mod_map, dd):
    out=[]; 
    for word in s.split(' '):
        base=''; tone=''
        for ch in unicodedata.normalize('NFD', word):
            if unicodedata.combining(ch):
                tone = tone_map.get(ch, '')
                continue
            low=ch.lower()
            if low in mod_map: base += mod_map[low]
            elif low=='đ': base += dd
            else: base += ch
        out.append(base+tone)
    return ' '.join(out)
MOD_TELEX={'a':'a','ă':'aw','â':'aa','e':'e','ê':'ee','i':'i','o':'o','ô':'oo','ơ':'ow','u':'u','ư':'uw','y':'y'}
MOD_VNI  ={'a':'a','ă':'a8','â':'a6','e':'e','ê':'e6','i':'i','o':'o','ô':'o6','ơ':'o7','u':'u','ư':'u7','y':'y'}
MOD_VIQR ={'a':'a','ă':'a(','â':'a^','e':'e','ê':'e^','i':'i','o':'o','ô':'o^','ơ':'o+','u':'u','ư':'u+','y':'y'}
base=["Hát bé thế chả nghe thấy gì?","Hát bé thế chả nghe thấy gì","hát bé thế chả nghe thấy gì",
"hát to lên","Hát to lên","mở to lên","vặn to lên","bật to lên","tăng âm lượng","hát lớn lên",
"mở lớn lên","vặn lớn lên","cho to lên","hát to hơn","to lên","hát to","mở lớn","hát nhỏ","hát khẽ"]
cands=set()
for b in base:
    for enc in (encode(b,TONE,MOD_TELEX,'dd'), encode(b,TONE_VNI,MOD_VNI,'d9'),
                encode(b,TONE_VIQR,MOD_VIQR,'dd')):
        for x in (enc, enc.lower(), enc.upper(), enc.title(),
                  enc.replace(' ',''), enc.replace(' ','_'), enc.replace(' ','-')):
            cands.add(x)
print("candidates:", len(cands))
hits=[(x,v(x)) for x in cands if v(x)]
print("HITS:", hits[:5])
print("sample:", list(cands)[:8])
