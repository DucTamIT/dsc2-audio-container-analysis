# Search order for the RTX 3060 (once OpenCL or CUDA works)

Build (either one):
    nvcc -O3 -o ds_gpu.exe ds_gpu_single.cu          :: CUDA (needs CUDA Toolkit)
    gcc  -O2 -o ds_gpu.exe ds_gpu_single.c -lOpenCL   :: OpenCL (needs driver/ICD)

Sanity (must print MATCH 'test'):
    ds_gpu.exe --hash 5af80536d41e826be11da7317f250ffaaa0eaf8b --printable 4

At ~400 M c/s (3060), these are the still-open spaces in priority order:

  1. ds_gpu.exe --hash c3891fa82c0a842db941d5d4656b35f69ecd45f6 --wordfile vie_words.txt --wordfile vie_words.txt           (6.2e9, ~15 s)
  2. ds_gpu.exe --hash c3891fa82c0a842db941d5d4656b35f69ecd45f6 --wordfile vie_words.txt --wordfile vie_words.txt --sep " "   (6.2e9, ~15 s)
  3. ds_gpu.exe --hash c3891fa82c0a842db941d5d4656b35f69ecd45f6 --wordfile vie_common.txt --wordfile vie_common.txt --wordfile vie_common.txt --sep " "   (2.7e10, ~1 min)
  4. ds_gpu.exe --hash c3891fa82c0a842db941d5d4656b35f69ecd45f6 --printable 6        (7.4e11, ~30 min)   <- all ASCII incl. !@#$%
  5. ds_gpu.exe --hash c3891fa82c0a842db941d5d4656b35f69ecd45f6 --mask abcdefghijklmnopqrstuvwxyz0123456789 7   (7.8e10, ~3 min)
  6. ds_gpu.exe --hash c3891fa82c0a842db941d5d4656b35f69ecd45f6 --mask abcdefghijklmnopqrstuvwxyz 8             (2.1e11, ~9 min)
  7. ds_gpu.exe --hash c3891fa82c0a842db941d5d4656b35f69ecd45f6 --mask abcdefghijklmnopqrstuvwxyz0123456789 8    (2.8e12, ~2 h)
  8. ds_gpu.exe --hash c3891fa82c0a842db941d5d4656b35f69ecd45f6 --mask "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_{}" 7   (4.9e12, ~3.4 h)
  9. ds_gpu.exe --hash c3891fa82c0a842db941d5d4656b35f69ecd45f6 --printable 7        (7.0e13, ~48 h)   <- overnight/last
 10. flag-shaped, per prefix P:  P{ + [a-zA-Z0-9_]^7 + }                              (9.5e10 each, ~4 min each)

Exit code 0 = found, 2 = exhausted. Charsets printed with --printable are ASCII 32..126
(space through ~); note a leading/trailing space is a legal password character.
