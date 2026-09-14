#!/usr/bin/env python3
"""
Extract file(s) from a DeepSound DSC2 ("v2 / 2024") container.

    python3 ds_extract.py <password> "<carrier.wav>" [outdir]

Verifies the password against the key-check in the header, decodes the payload
(mode 8 / 4 / 2), AES-256-CBC decrypts it with
  key = SHA256(UTF-16LE(password)), iv = key[:16]
and then walks the DeepSound file-info blocks, writing out every embedded file.

Requires: pycryptodome  (pip install pycryptodome)
"""
import hashlib
import os
import struct
import sys

try:
    from Crypto.Cipher import AES
except ImportError:
    sys.exit("need pycryptodome:  pip install pycryptodome")

MAGIC2 = b"DSC2"
MAGICS = (b"DSC2", b"DSCF")


def key_from_password(pw: str) -> bytes:
    return hashlib.sha256(pw.encode("utf-16-le")).digest()


def keycheck(key: bytes) -> bytes:
    from Crypto.Util.Padding import pad
    ct = AES.new(key, AES.MODE_CBC, key[:16]).encrypt(pad(key, 16))
    return hashlib.sha1(ct).digest()


def find_data_chunk(buf: bytes) -> int:
    """Return the byte offset where the audio 'data' payload begins."""
    i = buf.find(b"data")
    while i != -1:
        size = struct.unpack_from("<I", buf, i + 4)[0]
        if 8 + i + size <= len(buf) + 64 and size > 1000:
            return i + 8
        i = buf.find(b"data", i + 1)
    raise SystemExit("could not locate the WAV data chunk")


def decode(buf: bytes, mode: int) -> bytes:
    """DeepSound payload decode."""
    out = bytearray()
    if mode == 8:                      # high: 2 bits from bytes 0,2,4,6 of each 8
        for i in range(0, len(buf) - 7, 8):
            out.append(((buf[i] & 3) << 6) | ((buf[i+2] & 3) << 4) |
                       ((buf[i+4] & 3) << 2) | (buf[i+6] & 3))
    elif mode == 4:                    # normal: nibbles from bytes 0,2 of each 4
        for i in range(0, len(buf) - 3, 4):
            out.append(((buf[i] & 0x0F) << 4) | (buf[i+2] & 0x0F))
    elif mode == 2:                    # low: whole byte at even offsets
        for i in range(0, len(buf) - 1, 2):
            out.append(buf[i])
    else:
        raise SystemExit(f"unsupported mode {mode}")
    return bytes(out)


def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__)
        return 1
    pw, wav = sys.argv[1], sys.argv[2]
    outdir = sys.argv[3] if len(sys.argv) > 3 else "deepsound_out"
    raw = open(wav, "rb").read()
    base = find_data_chunk(raw)

    header = raw[base:base + 104]
    hdr = bytes(((header[i] & 0x0F) << 4) | (header[i + 2] & 0x0F)
                for i in range(0, 104, 4))
    magic, mode, enc, verify = hdr[:4], hdr[4], hdr[5], hdr[6:26]
    print(f"container : {magic!r}  mode={mode}  encrypted={enc}")
    print(f"keycheck  : {verify.hex()}")

    key = key_from_password(pw)
    if keycheck(key) != verify:
        print("password does NOT match this container")
        return 2
    print("password  : OK")

    # NOTE: the key-check uses AESUtils (CBC + PKCS7, IV = key[:16]), but the
    # PAYLOAD is encrypted with the Coder's own Rijndael instance, which the IL
    # sets to Mode=ECB and Padding=None.  Verified against a known container.
    pos = base + 104
    ecb = AES.new(key, AES.MODE_ECB)
    first = ecb.decrypt(decode(raw[pos:pos + 32 * mode], mode))
    plain = first
    print(f"first block: {plain[:32]!r}")
    try:
        print(f"           : {plain[:32].decode('utf-8', 'replace')}")
    except Exception:
        pass
    if plain[:4] not in (b"DSSF",):
        print("no DSSF file-info marker in the first block; dumping decrypted payload")
        blob = decode(raw[pos:], mode)
        dec = AES.new(key, AES.MODE_CBC, key[:16]).decrypt(blob)
        open(os.path.join(outdir, "payload.bin"), "wb").write(dec)
        print(f"wrote {outdir}/payload.bin ({len(dec)} bytes) for manual inspection")
        return 0

    os.makedirs(outdir, exist_ok=True)
    n = 0
    while True:
        block = decode(raw[pos:pos + 32 * mode], mode)
        if len(block) < 32:
            break
        info = ecb.decrypt(block)
        if info[:4] != b"DSSF":
            break
        name = info[4:24].split(b"\x00")[0].decode("utf-8", "replace").replace("?", "X")
        size = struct.unpack(">I", info[24:28])[0]      # big-endian
        pos += 32 * mode
        need = (size + 15) // 16 * 16                    # ciphertext is block aligned
        data = ecb.decrypt(decode(raw[pos:pos + need * mode], mode))[:size]
        dest = os.path.join(outdir, os.path.basename(name) or f"file{n}")
        open(dest, "wb").write(data)
        print(f"extracted : {dest}  ({len(data)} bytes)")
        n += 1
        pos += size * mode
    print(f"done, {n} file(s) in {outdir}/")
    return 0


if __name__ == "__main__":
    sys.exit(main())
