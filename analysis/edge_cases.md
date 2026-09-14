# KDF edge-case audit (real DeepSound DLL vs our implementation)

Computed with the shipping `Utils.dll` (`Jospin.Utils.Security.AESUtils.EncryptData`)
via Mono, and with the Python implementation, on the same inputs. **All 18 cases
match byte-for-byte**, and `123` reproduces the key-check of the known container
(`1756eb34…207b`).

| input | UTF-16 bytes | ciphertext | SHA1 (key-check) |
| --- | --- | --- | --- |
| `abc` | 6 | 48 | `ba8003593eff7c5ded33f48ab57c2810ef4a344d` |
| `abcdefg` | 14 | 48 | `eb61ae9069d672bf784f88086d97734cb96f2ee9` |
| `abcdefgh` | 16 | 48 | `dd7c55e75ad29a1b88484f1d9c273c3b0a51838a` |
| `a`×15 | 30 | 48 | `835c1841b281acca08a4249f6c993d7fd68ff7b5` |
| `a`×16 | 32 | 48 | `cc0dfad5a895cd44ceb7809027b27fad877781f8` |
| `a`×17 | 34 | 48 | `eced5b0cbddf9847332898f24a282ff8897af361` |
| `a`×31 | 62 | 48 | `73d8fc8b356b3bf6b29a058f15747a2ca05c02c1` |
| `a`×32 | 64 | 48 | `84e1f588681f63b40bb65d0c64c6bb110420a0bd` |
| `a`×33 | 66 | 48 | `83435c91a89863986a32e5a0b70f80222b9fa3bf` |
| `a`×64 | 128 | 48 | `753270c94856ea50ca90e7de228dbaa8126822bf` |
| `a`×100 | 200 | 48 | `d563bf309118bfa1ce24e4c5b157d999be5562fc` |
| `hát bé` | 12 | 48 | `5b786f06b42bb9fc452f1c1cf8fb34e9f2a1bc15` |
| `😀` | 4 | 48 | `f3ea3cd7b5ae3eb4462249ec647939335f6b5fc5` |
| `a😀b` | 8 | 48 | `656ced08dc3655998d77fe1df4e93acd67e5ae8a` |
| `ab\0cd` | 10 | 48 | `f4979fdbbe36a8cfd67bb6a3346666f6de111bd9` |
| `a\tb` | 6 | 48 | `27723548f6381b6ca9c4b74b8fb9e44f1af7a84a` |
| `a b` | 6 | 48 | `c3d269b7e51e8a19bfa6e771cbe1549b6122ca1f` |
| `123` | 6 | 48 | `1756eb343053c3e27e713e53b1f35313f7d9207b` |

Conclusion: no length limit, no truncation, no encoding drift. UTF-16LE with
surrogate pairs, embedded NULs, tabs and spaces are all handled identically to the
real tool, and the ciphertext is always 48 bytes (32-byte key + one PKCS7 block).

## rockyou.txt sanity

* 14 344 391 lines; first entries `123456, 12345, 123456789, password, iloveyou`
  (the standard Kali list).
* 14 534 lines contain bytes ≥ 0x80 and **218 are invalid UTF-8**. All were tested
  under five decodings (`utf-8` ignore / replace / surrogateescape, `latin-1`,
  `cp1252`) — no match, so the non-ASCII corner of the list is covered.
