# DeepSound DSC2 audio container — format analysis and verification tooling

Private working notes and tooling for examining a **DeepSound `DSC2`** audio
container: header/KDF documentation, an independent verifier, and a candidate
search harness used to check derivations of the challenge wording.

## The target

| item | value |
| --- | --- |
| carrier | `Con chim non.wav` — 16-bit stereo 44.1 kHz PCM, 129 003 502 bytes, 731.31 s |
| md5 | `c5579bdcce8a67757b644ea7969cad3c` |
| sha256 | `19c88fc8b94684e773382ee69bdb198e0ee9bff18013007fb323f4b3945fe22d` |
| crc32 | `F6F4F5C4` |
| container | DeepSound 2.x **`DSC2`** (DeepSound 2.3 reports "data format v2 (2024)"), starts at file offset **46** (first byte of the `data` chunk) |
| header (26 decoded bytes) | `DSC2` · mode `8` (high) · encrypted `1` · key-check `c3891fa82c0a842db941d5d4656b35f69ecd45f6` |
| payload | starts at raw offset **150** (46 + 104), 2 payload bits per sample, 16 125 437 bytes capacity |

DeepSound stores the header in the low nibble of every *other* byte, decoded with
its "normal" rule `out[i] = (buf[4i] & 0x0F) << 4 | (buf[4i+2] & 0x0F)`; the payload
("high" mode 8) takes 2 bits from bytes 0,2,4,6 of every 8-byte group.

## The stored verification block

```
c3891fa82c0a842db941d5d4656b35f69ecd45f6
```

KDF (from `Jospin.DeepSound.Steganography.Fragile.Coder::set_KeyUnicode`,
verified against the shipping `Utils.dll` of DeepSound 2.2 **and** 2.3):

```
key      = SHA256( UTF-16LE(password) )
c0       = AES256_key( 00*16 )
c1       = AES256_key( key[16:32] ^ c0 )
c2       = AES256_key( 10*16 ^ c1 )          # PKCS7 block
keycheck = SHA1( c0 || c1 || c2 )
payload  = AES-256-CBC( key, iv = key[:16], PKCS7 ) @ raw offset 150
```

Legacy scheme (only if the magic were `DSCF`): `SHA1(password null-padded to 32)`,
which is exactly stock John's `dynamic_1529`.

### Self-test vectors (must match)

| password | keycheck |
| --- | --- |
| `test` | `5af80536d41e826be11da7317f250ffaaa0eaf8b` |
| `gain` | `35a42fb91ff63d3bd4b4f318666f252c5d398b4f` |
| `` (empty) | `ccb7867e119a42b48fa97b19fb15634fd16e226e` |
| `hát bé` | `5b786f06b42bb9fc452f1c1cf8fb34e9f2a1bc15` |

## Build (Windows)

Single file, no other dependencies. `--printable` covers ASCII 32..126
(space through `~`).

```bat
:: CUDA (needs the CUDA Toolkit -> winget install --id Nvidia.CUDA -e)
nvcc -O3 -o ds_gpu.exe src\ds_gpu_single.cu

:: or OpenCL (needs the vendor ICD; see NO_OPENCL.md)
gcc -O2 -o ds_gpu.exe src\ds_gpu_single.c -lOpenCL
cl  /O2 src\ds_gpu_single.c /I"%CUDA_PATH%\include" /link /LIBPATH:"%CUDA_PATH%\lib\x64" OpenCL.lib
```

Sanity checks — both must print a match:

```bat
ds_gpu.exe --list
ds_gpu.exe --hash 5af80536d41e826be11da7317f250ffaaa0eaf8b --printable 4
ds_gpu.exe --hash 689318a8b679669b9f92b6292af245171d82e4d4 --prefix "miniCTF{" --suffix "}" --printable 4
```

## Run order (see SEARCH_PLAN.md for the full table)

Priority order, all still open. At ~400 M c/s on a 3060:

```bat
ds_gpu.exe --hash c3891fa82c0a842db941d5d4656b35f69ecd45f6 --wordlist wordlists\vie_words.txt --wordlist wordlists\vie_words.txt
ds_gpu.exe --hash c3891fa82c0a842db941d5d4656b35f69ecd45f6 --printable 6
ds_gpu.exe --hash c3891fa82c0a842db941d5d4656b35f69ecd45f6 --mask abcdefghijklmnopqrstuvwxyz0123456789 7
ds_gpu.exe --hash c3891fa82c0a842db941d5d4656b35f69ecd45f6 --mask abcdefghijklmnopqrstuvwxyz 8
ds_gpu.exe --hash c3891fa82c0a842db941d5d4656b35f69ecd45f6 --mask abcdefghijklmnopqrstuvwxyz0123456789 8
ds_gpu.exe --hash c3891fa82c0a842db941d5d4656b35f69ecd45f6 --mask "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_{}" 7
ds_gpu.exe --hash c3891fa82c0a842db941d5d4656b35f69ecd45f6 --printable 7
```

Exit code `0` = found, `2` = space exhausted, so the list can be chained.
`--pos CS` (repeatable) gives one charset per position; `--prefix/--suffix` add
fixed text around the generated part (e.g. `--prefix "ASCIS{" --suffix "}"`).

Other modes that already produced nothing here and are cheap to re-run:

```bat
ds_gpu.exe --hash c3891fa82c0a842db941d5d4656b35f69ecd45f6 --wordlist rockyou.txt
ds_gpu.exe --hash c3891fa82c0a842db941d5d4656b35f69ecd45f6 --wordlist wordlists\candidates.txt
```

## Already eliminated (do not redo)

| space | candidates | result |
| --- | --- | --- |
| all printable ASCII ^1..6 | 7.35e11 | NOT FOUND (M1 Pro, 32 277 s) |
| `[a-z0-9]^1..6` | 2.24e9 | NOT FOUND |
| `[a-z]^7` | 8.03e9 | NOT FOUND |
| `[a-zA-Z0-9_{}]^4..5` | 1.18e9 | NOT FOUND |
| Vietnamese 2-word (77 k dict, `""` and `" "`) | 5.94e9 ×2 | NOT FOUND |
| Vietnamese 3-word (common 3000, `""` and `" "`) | 2.7e10 ×2 | NOT FOUND |
| rockyou (14.3 M), rockyou × 287 M rules, Vietnamese dict + affixes, public breach-compilation wordlists, macOS English dictionary, Vietnamese dehashed 384 k, note-letter melody, TELEX/VNI/VIQR input forms, teencode, idioms, metadata-derived | ~1.5e9 | NOT FOUND |
| John `--wordlist=rockyou.txt --rules` with the custom `deepsound2` format | 1.1 M c/s | NOT FOUND |

**Conclusion so far:** any printable-ASCII password is ≥ 7 characters; lowercase-only
and alnum are ≥ 7; `[a-zA-Z0-9_{}]` is ≥ 6.

## Content channels already proven empty

Everything below was actually run on this WAV — no need to repeat:

* Spectrogram: whole file and 148 rendered tiles (74 × 10 s of the full mix and
  74 × 10 s of **L−R**), 5–12 kHz band-limited render with per-bin steady-tone
  removal, noise-floor (sliding-minimum) render, phase and instantaneous-frequency
  renders, extreme zooms of the intro (0–2 s) and outro (729–731.3 s). No glyphs,
  QR, or text anywhere.
* Bit planes: bits 0–3 of L/R, both bit orders, strides 1/2/4, cross-channel
  mixed-bit (L bit i, R bit j) — no text; the same bit planes rendered as
  *signals* are flat white noise.
* Audio: no speech (Whisper small/medium, whole file, 73 × 10 s windows, the 20
  quietest sections amplified, reversed audio, 2×/4× speed); no SSTV (1200 Hz
  pulses irregular at ~1.5 s, SSTV needs a regular 0.12–0.15 s); no DTMF; no
  custom dual-tone grid (dominant tones spread, not a lattice); no Morse
  (300 Hz–8 kHz × 3 thresholds); no UART square wave; no waveform-binary
  (RMS unimodal); no rhythm/onset code (intervals quantise to a musical grid,
  accents unimodal); no analog raster (envelope autocorrelation peaks only at
  the music's own period).
* Literal text: `strings` (ASCII and UTF-16LE), UTF-8 multibyte, low-byte and
  high-byte streams, DeepSound-style nibble decodes at all 4 phases — the only
  readable string in the entire file is the 26-byte DeepSound header itself.
* Container: RIFF fully accounted (no trailing data, no extra chunks), 7z has no
  appended data and no comment, payload has no `DSSF` marker and no plaintext.

So the flag lives behind that one password; the DeepSound payload is 16 MB of
AES-CBC ciphertext and cannot be read without it.

## Validated container format (checked against a known container)

A second container (`MCK.wav`, verif `1756eb343053c3e27e713e53b1f35313f7d9207b`)
was used to validate the implementation end to end. Its password is `123`
(rockyou line 3985), which reproduces the stored key-check exactly and extracts a
valid file. Two corrections came out of that:

| aspect | value |
| --- | --- |
| key-check cipher | **AES-256-CBC, PKCS7, IV = key[:16]** (`AESUtils.EncryptData`) |
| **payload cipher** | **AES-256-ECB, no padding** — the Coder's own Rijndael instance is constructed with `Mode=2` (ECB) and `Padding=1` (None) |
| info block | `DSSF` + 20-byte NUL-padded file name + 4-byte **big-endian** size, then `size * mode` raw carrier bytes |
| name padding | `?` is replaced by `X` by the tool; NUL padding is stripped |

So the key-check and the payload use *different* ciphers — a trap worth knowing.

## Extracting once the password is found

```bash
python3 scripts/ds_extract.py "THE_PASSWORD" "/path/Con chim non.wav"
```

It verifies the key against `c3891f…45f6`, decodes the mode-8 payload from offset
150, AES-CBC-decrypts it, prints the first blocks, and parses/dumps any embedded
file-info blocks and file data.

## John the Ripper

`src/deepsound2_fmt_plug.c` implements the DSC2 KDF as a John format
(UTF-8 → UTF-16LE, self-registering with test vectors, ~1.1 M c/s). Drop it into
`john/src/`, `make -sj4`, then:

```
john --format=deepsound2  --wordlist=rockyou.txt --rules hash_dsc2.txt
john --format=dynamic_1529 --wordlist=rockyou.txt        hash_dscf.txt   # legacy scheme
```

## Repo layout

```
src/         container/KDF sources (modular + single-file CUDA/OpenCL) and the John format
wordlists/   Vietnamese dictionary (77 k), common 3000, numbers, 71 hand-picked guesses
scripts/     run_all.sh / run_all2.sh / run_prefix.sh / run_braces.sh / run_words.sh
             and ds_extract.py
analysis/    the analysis scripts used for the audio/encoding sweeps
logs/        (created by the search scripts) durable per-step result logs
```

The 129 MB WAV is deliberately **not** in the repo; verify it by the hashes above.
