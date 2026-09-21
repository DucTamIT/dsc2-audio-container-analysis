import hashlib
from Crypto.Cipher import AES
from Crypto.Util.Padding import pad

TARGET = bytes.fromhex('c3891fa82c0a842db941d5d4656b35f69ecd45f6')

def check(pw):
    key = hashlib.sha256(pw.encode('utf-16-le')).digest()
    c = AES.new(key, AES.MODE_CBC, key[:16]).encrypt(pad(key, 16))
    return hashlib.sha1(c).digest() == TARGET

hints = [
    'rockyou.txt - sản phẩm không mong muốn của ông Tokuda',
    'rockyou.txt - san pham khong mong muon cua ong Tokuda',
    'rockyou.txt - san pham khong mong muon cua ong tokuda',
    'rockyou.txt - sản phẩm không mong muốn của ông tokuda',
    'rockyou.txt-sản phẩm không mong muốn của ông Tokuda',
    'rockyou.txt-san pham khong mong muon cua ong tokuda',
    'sản phẩm không mong muốn của ông Tokuda',
    'san pham khong mong muon cua ong tokuda',
    'san pham khong mong muon cua ong Tokuda',
    'sanphamkhongmongmuoncuaongtokuda',
    'sanphamkhongmongmuoncuaongTokuda',
    'Sanphamkhongmongmuoncuaongtokuda',
    'SANPHAMKHONGMONGMUONCUAONGTOKUDA',
    'sản phẩm không mong muốn',
    'san pham khong mong muon',
    'sanphamkhongmongmuon',
    'ông Tokuda',
    'ong Tokuda',
    'ong tokuda',
    'ongtokuda',
    'ongTokuda',
    'Lance Tokuda',
    'lance tokuda',
    'lancetokuda',
    'LanceTokuda',
    'LANCETOKUDA',
    'Shigeo Tokuda',
    'shigeo tokuda',
    'shigeotokuda',
    'ShigeoTokuda',
    'rockyou.txt',
    'rockyou',
    'RockYou',
    'ROCKYOU',
    'Rockyou',
    'rockyou2009',
    'RockYou2009',
    'rockyou_2009',
    'rockyou2026',
    'RockYou2026',
    'rockyou_2026',
    'rockyou!',
    'RockYou!',
    'rockyou@',
    'rockyou#',
    'rockyou123',
    'RockYou123',
    'Hãy thử nghiên cứu về âm thanh thật sâu vào, I mean DeepSound (")>',
    'DeepSound (")>',
    'DeepSound',
    'deepsound',
    'DEEPSOUND',
    'Hát bé thế chả nghe thấy gì?',
    'hát bé thế chả nghe thấy gì?',
    'Hát bé thế chả nghe thấy gì',
    'hát bé thế chả nghe thấy gì',
    'hat be the cha nghe thay gi?',
    'hat be the cha nghe thay gi',
    'hatbethechanghethaygi?',
    'hatbethechanghethaygi',
    'Hatbethechanghethaygi?',
    'Hatbethechanghethaygi',
    'HATBETHECHANGHETHAYGI',
    'Liên khúc Con chim non',
    'lien khuc con chim non',
    'lienkcucconchimnon',
    'Con chim non',
    'con chim non',
    'conchimnon',
    'ConChimNon',
    'CONCHIMNON',
    'Bé Xuân Mai',
    'bé xuân mai',
    'be xuan mai',
    'bexuanmai',
    'BeXuanMai',
    'Xuan Mai',
    'xuan mai',
    'xuanmai',
    'The Voice 2026',
    'the voice 2026',
    'thevoice2026',
    'TheVoice2026',
]

print(f'Testing {len(hints)} direct hint strings...')
for h in hints:
    if check(h):
        print('*** MATCH FOUND! ***:', repr(h))
        exit(0)
    for p in ['miniCTF{', 'minictf{', 'flag{', 'FLAG{']:
        candidate = f'{p}{h}}}'
        if check(candidate):
            print('*** MATCH FOUND! ***:', repr(candidate))
            exit(0)
print('None matched.')
