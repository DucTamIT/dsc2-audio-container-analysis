# Does the algorithm differ between DeepSound 2.1 / 2.2 / 2.3?

**No.** 2.1, 2.2 and 2.3 produce byte-identical results. 2.0 is the only version
with a different scheme, and our container is `DSC2`, which 2.0 cannot even write.

## IL-level comparison (extracted from each release's own DLLs)

| version | source | `Coder::set_KeyUnicode` | `AESUtils::EncryptData` |
| --- | --- | --- | --- |
| 2.0 (`v2.0.0.32372`) | `DeepSoundSetup.msi` | **absent** | **absent** |
| 2.1 (`v2.1.2401.03`) | `DeepSound_2_1.msi` | present, md5 `5ff12804d4fbca14ea2c683e4777b3bd` | present, md5 `e54ef1f60922ecef3bab2717f4fa0bac` |
| 2.2 (`v2.2.2404.04`) | `DeepSound_2_2_2404_14_Setup.msi` | **identical** (same md5) | **identical** (same md5) |
| 2.3 (`v2.3.2606.28`) | `DeepSoundBinary_2_3.zip` | **identical** (same md5) | **identical** (same md5) |

`set_KeyUnicode` is the whole DSC2 key schedule; `EncryptData` is the CBC+PKCS7
primitive it calls. Both unchanged across 2.1 → 2.3.

2.0 has only `Coder::set_Key` (ASCII, 32-byte zero-padded buffer → `SHA1`), i.e.
the legacy `DSCF` scheme that `deepsound2john.py` targets with `$dynamic_1529$`.

## Live comparison — calling each release's own AES routine

Compiled the same program against each release's `Utils.dll` and ran it:

| password | 2.0 | 2.1 | 2.2 | 2.3 | ours |
| --- | --- | --- | --- | --- | --- |
| `test` | n/a (`AESUtils` does not exist) | `5af80536d41e826be11da7317f250ffaaa0eaf8b` | same | same | same |
| `gain` | n/a | `35a42fb91ff63d3bd4b4f318666f252c5d398b4f` | same | same | same |
| `123` | n/a | `1756eb343053c3e27e713e53b1f35313f7d9207b` | same | same | same |
| `hát bé` | n/a | `5b786f06b42bb9fc452f1c1cf8fb34e9f2a1bc15` | same | same | same |

Ciphertext length is 48 bytes in every case (32-byte key + one PKCS7 block).

## Conclusion

The KDF is stable from 2.1 onward. A `DSC2` container is always
`SHA256(UTF-16LE(pw))` + `SHA1(AES-256-CBC(key, IV=key[:16], PKCS7(key)))`,
regardless of whether it was written by 2.1, 2.2 or 2.3 — so the version question
cannot explain a failed candidate. (Payload data is separate: it uses the Coder's
own Rijndael instance with ECB/NoPadding, see README.)
