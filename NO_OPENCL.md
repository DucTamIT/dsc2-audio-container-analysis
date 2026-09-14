# No OpenCL on the machine? Two ways forward

The RTX 3060 itself is fine — what's missing is the **OpenCL ICD** (the
`OpenCL.dll` loader and/or NVIDIA's `nvopencl64.dll`). Recent NVIDIA drivers do
normally install both; some OEM/laptop or "DCH" packages leave them out.

## First, confirm what is actually there

Open a normal cmd and run:

```bat
dir C:\Windows\System32\OpenCL.dll
dir C:\Windows\System32\nvopencl*.dll
reg query "HKLM\SOFTWARE\Khronos\OpenCL\Vendors"
nvidia-smi
```

* `nvidia-smi` works but `OpenCL.dll` / the `Vendors` key is missing → the
  runtime was never installed.
* `nvopencl64.dll` exists but `OpenCL.dll` does not → only the ICD loader is
  missing.

## Fix A — install the OpenCL runtime (keeps the OpenCL build)

1. **CUDA Toolkit** (contains `nvopencl64.dll`, `OpenCL.dll`, `CL/cl.h` and
   `OpenCL.lib` — everything needed to compile *and* run):
   ```bat
   winget install --id Nvidia.CUDA -e
   ```
   or download from developer.nvidia.com/cuda-downloads. Reboot afterwards.
2. **Or** install the official NVIDIA driver "clean" from nvidia.com
   (Studio/Game Ready), which bundles OpenCL.
3. Re-run the four checks above; then:
   ```bat
   ds_gpu.exe --list
   ```

## Fix B — skip OpenCL entirely, use CUDA

`ds_gpu_single.cu` is the same search tool rewritten for CUDA, and it is a **single
file** — one command, no include paths, no separate kernel file:

```bat
nvcc -O3 -o ds_gpu.exe ds_gpu_single.cu
```

You need the CUDA Toolkit for `nvcc` (see Fix A step 1 — installing it also
gives you OpenCL, so either build then works).

Verify (must print `test` and `miniCTF{test}`):

```bat
ds_gpu.exe --list
ds_gpu.exe --hash 5af80536d41e826be11da7317f250ffaaa0eaf8b --mask abcdefghijklmnopqrstuvwxyz 4
ds_gpu.exe --hash 689318a8b679669b9f92b6292af245171d82e4d4 --prefix "miniCTF{" --suffix "}" --mask abcdefghijklmnopqrstuvwxyz 4
```

Then attack the real hash:

```bat
ds_gpu.exe --hash c3891fa82c0a842db941d5d4656b35f69ecd45f6 --mask abcdefghijklmnopqrstuvwxyz0123456789 8
ds_gpu.exe --hash c3891fa82c0a842db941d5d4656b35f69ecd45f6 --prefix "YourCTF{" --suffix "}" --mask abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_ 7
```

Identical CLI to the OpenCL build: `--hash`, `--wordlist`, `--mask CS LEN`,
`--pos CS` (repeatable), `--prefix`, `--suffix`, `--batch N`, `--device N`,
`--list`. Exit code `0` = found, `2` = exhausted.

## Fix C — no GPU runtime at all

The CPU search tools in `deepsound/brute/` need nothing but a C compiler:

```bash
gcc -O3 -march=native -o fast2 fast2.c -lcrypto    # ~1.5 M/s
./fast2 7 8                                        # 7 chars, [a-z0-9]
```

and the Python/John routes also work (`ds_gpu_single.c` is GPU-only; the
`deepsound2` John format is CPU and runs anywhere).

## What to send me if it still fails

* output of `nvidia-smi`
* output of `dir C:\Windows\System32\OpenCL.dll` and the `reg query` line
* the exact `nvcc --version` or `cl`/`gcc` error text
