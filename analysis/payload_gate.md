# Independent check: is the key-check field genuine, or a decoy?

Motivation: if the stored 20-byte key-check were random filler, no password would
ever verify and the "right" attack would be to decrypt the **payload** directly.
That is testable without the key-check, because a correct key makes the first
payload block decode to the `DSSF` file-info marker (see the payload cipher note:
AES-256-ECB, no padding).

## Gate validation on the known container

`MCK.wav`, password `123`:

| cipher used on the first payload block | first 4 bytes |
| --- | --- |
| ECB (what the IL says) | **`DSSF`** ✔ |
| CBC (alternative) | `b\x85\xfb\xeb` ✗ |

So ECB is right and the gate is a valid, independent criterion — it costs one AES
block per candidate instead of three plus a SHA-1.

## Applied to the target

`Con chim non.wav`, first payload block at raw offset 150 → 256 raw bytes →
32 decoded bytes; test `AES-256-ECB(SHA256(UTF-16LE(pw))).decrypt(block)[:4] == "DSSF"`:

| wordlist | candidates | verif gate | payload gate |
| --- | --- | --- | --- |
| rockyou.txt | 14 344 391 | NOT FOUND | **NOT FOUND** |

Both criteria agree, so the key-check is **not** a decoy: the container really is
password-protected with the standard DSC2 KDF, and a failed candidate is a genuine
non-match on both counts. No need to distrust the verifier.
