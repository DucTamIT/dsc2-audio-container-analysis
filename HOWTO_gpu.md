# DeepSound DSC2 audio container — analysis toolkit — how to run

Everything is in **one file**: `ds_gpu_single.c` (the KDF and the OpenCL kernel are
embedded). You do not need `ds_kdf.h` or `ds_gpu.cl` for this build.

Target (the DeepSound key-check of `Con chim non.wav`):

```
c3891fa82c0a842db941d5d4656b35f69ecd45f6
```

---

## Step 0 — OpenCL

* **Windows**: the NVIDIA driver already installs the OpenCL *runtime*. To
  compile you also need headers + import library, which come with the **CUDA
  Toolkit** (default path `C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\vX.Y`).
  Alternatively grab the Khronos `CL/` headers and NVIDIA's `OpenCL.lib`.
* **Linux**: `sudo apt install ocl-icd-opencl-dev`
* **macOS**: built in, nothing to install.

## Step 1 — build

**Windows, MSVC** (open "x64 Native Tools Command Prompt for VS"):

```
cl /O2 ds_gpu_single.c ^
   /I"C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.4\include" ^
   /link /LIBPATH:"C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.4\lib\x64" OpenCL.lib
```

**Windows, MinGW-w64:**

```
gcc -O2 -o ds_gpu.exe ds_gpu_single.c -lOpenCL
```

**Linux:**

```
gcc -O2 -o ds_gpu ds_gpu_single.c -lOpenCL
```

**macOS:**

```
clang -O2 -framework OpenCL -o ds_gpu ds_gpu_single.c
```

## Step 2 — is the 3060 visible?

```
./ds_gpu --list
```

You want to see something like `[0] NVIDIA GeForce RTX 3060 (12288 MB)`.
If you see nothing, the OpenCL runtime is missing.

## Step 3 — verify the build before trusting it (30 seconds)

These two commands must both print a match. They prove the whole chain
(UTF-16LE, SHA-256, AES-256, SHA-1, the kernel, the comparison) works on your GPU:

```
./ds_gpu --hash 5af80536d41e826be11da7317f250ffaaa0eaf8b --mask abcdefghijklmnopqrstuvwxyz 4
   -> *** MATCH: password = 'test' ***

./ds_gpu --hash 689318a8b679669b9f92b6292af245171d82e4d4 --prefix 'miniCTF{' --suffix '}' --mask abcdefghijklmnopqrstuvwxyz 4
   -> *** MATCH: password = 'miniCTF{test}' ***
```

## Step 4 — attack the real hash

```
# 8 chars, lower+digits  (2.8e12 -> ~2 h on a 3060)
./ds_gpu --hash c3891fa82c0a842db941d5d4656b35f69ecd45f6 \
         --mask abcdefghijklmnopqrstuvwxyz0123456789 8

# a wordlist, incl. Vietnamese with diacritics
./ds_gpu --hash c3891fa82c0a842db941d5d4656b35f69ecd45f6 --wordlist rockyou.txt

# flag-shaped: PREFIX{ inner }   (~4 min per prefix for 7 inner chars)
INNER='abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_'
./ds_gpu --hash c3891fa82c0a842db941d5d4656b35f69ecd45f6 \
         --prefix 'miniCTF{' --suffix '}' --mask "$INNER" 7
```

`gpu/run_braces.sh` runs the whole braces/mixed-case program in the right order and
stops the moment something matches (on Windows use `ds_gpu.exe` in that script).

## Exit codes

`0` = password found (and printed), `2` = space exhausted, `1` = usage/other error —
handy for chaining searches in a script.

## All options

| option | meaning |
| --- | --- |
| `--hash HEX40` | the 20-byte DeepSound DSC2 key-check |
| `--wordlist FILE` | try each line (UTF-8; Vietnamese diacritics handled) |
| `--mask CS LEN` | LEN positions, every position uses charset `CS` |
| `--pos CS` | one charset for one position, repeat left-to-right |
| `--prefix S` / `--suffix S` | fixed UTF-8 text around the generated part |
| `--device N` | pick GPU N (see `--list`) |
| `--cpu` | also allow CPU OpenCL devices |
| `--batch N` | candidates per launch (default 2^21; raise for more speed) |
| `--list` | list devices and exit |

Note: charsets passed to `--mask`/`--pos` are ASCII. For non-ASCII text use
`--prefix` / `--suffix`, or `--wordlist` (both take UTF-8).

## Where the password lives in the file (for the record)

WAV offset 46 (first byte of `data`), nibble-interleaved:

```
44 53 43 32 08 01 | c3 89 1f a8 2c 0a 84 2d b9 41 d5 d4 65 6b 35 f6 9e cd 45 f6
"DSC2"      mode=8  encrypted=1        key-check (verification block under test)
```

Payload starts at offset 150; once you have the password:

```python
import hashlib
from Crypto.Cipher import AES
key = hashlib.sha256(password.encode("utf-16-le")).digest()
aes = AES.new(key, AES.MODE_CBC, key[:16])
# the container body starts at file offset 150 (mode 8 -> 2 bits per sample)
```
