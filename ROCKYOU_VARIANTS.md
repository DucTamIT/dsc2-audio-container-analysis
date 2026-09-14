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
