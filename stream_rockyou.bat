@echo off
"C:\Program Files\7-Zip\7z.exe" e -so -mmt=on -bsp0 -bso0 "D:\rockyou2024.zip" | ds_gpu.exe --hash c3891fa82c0a842db941d5d4656b35f69ecd45f6 --wordlist - --batch 8388608
