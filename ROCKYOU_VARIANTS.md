# Bigger / other "rockyou" wordlists

Target hash (unchanged): `c3891fa82c0a842db941d5d4656b35f69ecd45f6`
(`Con chim non.wav`, DSC2, mode 8, verified KDF).

Run any list with:

```bat
ds_gpu.exe --hash c3891fa82c0a842db941d5d4656b35f69ecd45f6 --wordlist <list> --batch 33554432
```

Exit code `0` = found (prints `*** MATCH ***`), `2` = exhausted, so you can chain
them in a `.bat` loop and walk away. On a 3060 the wall time is usually set by
*disk read*, not the GPU — expect roughly 1 GB/s, i.e. ~2 min per 100 GB.

| # | list | lines | where | why it's worth a shot |
| --- | --- | --- | --- | --- |
| 1 | `rockyou.txt` | 14 344 391 | Kali `/usr/share/wordlists/rockyou.txt.gz` | **done here — NOT FOUND** (plain, ×JumboSingle rules, ×30 KDF variants, ×36 AES-128 variants, whitespace variants) |
| 2 | `rockyou-withcount.txt` | 14 344 391 | Kali `wordlists` pkg (withcount variant) | same passwords, but each line is `<count> <password>` — only matters if the author pasted a whole line |
| 3 | **RockYou2021** | **8 459 060 239** | `rockyou2021.txt`, ~100 GB, torrent / mega mirrors | the obvious "more rockyou"; roughly frequency-ordered, so a hit appears early |
| 4 | **RockYou2024** | **9 948 575 739** | mirrors as `rockyou2024.txt` | newer re-compilation of the same corpus |
| 5 | `xato-net-10-million-passwords.txt` | 10 000 000 | `https://raw.githubusercontent.com/danielmiessler/SecLists/master/Passwords/Common-Credentials/xato-net-10-million-passwords.txt` | independent 10 M list (verified reachable) |
| 6 | `xato-net-10-million-passwords-1000000.txt` | 1 000 000 | same folder | top-1 M slice (verified reachable) |
| 7 | `crackstation-human-only.txt.gz` | 63 995 298 | `https://crackstation.net/files/crackstation-human-only.txt.gz` | real-world human passwords (verified reachable) |
| 8 | `crackstation.txt.gz` | 1 493 677 782 | `https://crackstation.net/files/crackstation.txt.gz` | 1.5 B cracked hashes |
| 9 | Weakpass `3a` / `3` | 1 000 000 000+ | `https://weakpass.com/wordlist/1` (index) | large mixed corpus |
| 10 | **Kaonashi** | ~1 B+ | `https://github.com/kaonashi-passwords/Kaonashi` | **Vietnamese-oriented** wordlist/ruleset — a Vietnamese author is a real possibility |
| 11 | Vietnamese dehashed-ish lists | ~400 k–3 M | e.g. `vie.txt`, `vie_words.txt` (already in this repo) | small; already tested and negative |

`10-million-password-list-top-1000000.txt` is 404 on the current SecLists master
(the path moved) — use the `xato-net-*` files instead.

## If a list exhausts with no hit

Cheap derived passes, in order (all supported by the tool):

```bat
:: copied-from-file accidents
ds_gpu.exe --hash <H> --wordlist rockyou2021.txt --append "\r"
ds_gpu.exe --hash <H> --wordlist rockyou2021.txt --append " "
:: case/leet style mangling without a rule engine
ds_gpu.exe --hash <H> --mask abcdefghijklmnopqrstuvwxyz0123456789 7
ds_gpu.exe --hash <H> --printable 6
:: flag-shaped (needs the event prefix)
ds_gpu.exe --hash <H> --prefix "PREFIX{" --suffix "}" --mask "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_" 7
```

For really large rule sets (OneRuleToRuleThemAll, ~50 k rules) don't pipe through
John — its generator caps at ~1.25 M/s. Either add a GPU-side rules mode, or run
hashcat with a module (hashcat needs OpenCL, which is missing on this box).

## Notes

* `--wordlist` streams and strips `\r\n`; use `--append "\r"` if you need the CR.
* For a 100 GB file, split it (`split -l 500000000`) and run the parts in parallel
  on the GPU while the disk read is the limiter, or run one part per night.
* Every command above is byte-identical in behaviour to the verifier that opened
  `MCK.wav` with password `123`, so a hit here is trustworthy and a miss is real.

## RockYou2024 — sizes, parts and the low-disk workflow

Two independent distributions exist. The **size-sorted 7z is the one to use if
space is tight** — it is ~11 GB instead of ~50/146 GB, and it is already split by
password length so you can prioritise and stream.

| distribution | artifact | size | notes |
| --- | --- | --- | --- |
| `trstout/RockYou2024` (magnet `btih:4e3915a8ecf6bc174687533d93975b1ff0bde38a`) | `rockyou2024.zip` | **~46–50 GB** | one part; extracts to `rockyou2024.txt` |
| same | `rockyou2024.txt` | **~146–160 GB** | the raw list (2 parts in the torrent: zip + txt) |
| `tManser/RockYou2024-size-sorted` (magnet `btih:866e2005a716f35e2b7d534c322e0c98deef2549`, `xl=11308476707`) | `RockYou2024_size_sorted.7z` | **~11.3 GB** | **split by character length** into small files |

Both are torrents; also mirrored on Kaggle (`bwandowando/common-password-list-rockyou2024-txt`).
Checksums published with the `trstout` magnet: `rockyou2024.zip` sha256
`d3380267907a7aa7b6161010632add84ad6f25387915771a9c1f111932a20a19`,
`rockyou2024.txt` sha256 `457361a871f111014573ab3bda3e0f5dafd489a3217b62fc8cfb14c74d59bb11`.

### Process it without ever extracting 146 GB

The size-sorted archive contains files like
`RockYou2024 8 character passwords/8_Character_1.txt`, so you can extract one at a
time straight into the cracker:

```bat
7z l RockYou2024_size_sorted.7z
:: one inner file -> stdout -> GPU, no extraction to disk
7z x -so RockYou2024_size_sorted.7z "RockYou2024 8 character passwords/8_Character_1.txt" ^
  | ds_gpu.exe --hash c3891fa82c0a842db941d5d4656b35f69ecd45f6 --wordlist - --batch 33554432
```

Recommended length order (a DeepSound GUI password is usually 6–12 chars):
**8, 9, 7, 10, 6, 11, 12**, then the rest. Each length bucket is a few GB at most,
so peak disk use is the 11 GB archive.

From the full 146 GB list you can also stream directly if it is gzipped:

```bat
zcat rockyou2024.txt.gz | ds_gpu.exe --hash <H> --wordlist - --batch 33554432
```

### Time

Download: torrent, so it depends on seeds — `size / your line rate`, plus swarm
speed. The size-sorted 11 GB is typically 15–60 min on a decent connection; the
50 GB zip is hours.

Processing on a 3060 (~400 M/s) is not the bottleneck: 10 M ≈ instant, 64 M ≈ 1 s,
9.9 G ≈ 25 s of GPU — so **disk read (~1 GB/s) dominates**: ~2.5 min for 146 GB,
~12 s for 11 GB.

### Cheaper first (do these before any big download)

| list | lines | download | GPU time @400 M/s |
| --- | --- | --- | --- |
| `xato-net-10-million-passwords.txt` | 10 M | ~90 MB | instant |
| `xato-net-10-million-passwords-1000000.txt` | 1 M | ~9 MB | instant |
| `crackstation-human-only.txt.gz` | 64 M | ~500 MB | ~1 s |
| Kaonashi (Vietnamese) | ~1 B | repo | ~2.5 s |
| RockYou2024 size-sorted | 9.9 G | **~11 GB** | ~25 s + disk |
| RockYou2024 full | 9.9 G | ~50–146 GB | ~25 s + disk |
