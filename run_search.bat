@echo off
REM Ordered candidate search for the DSC2 container ("The Voice 2026").
REM Build first:  nvcc -O3 -o ds_gpu.exe src\ds_gpu_single.cu
REM Exit code 0 = found, 2 = space exhausted -> the loop stops on a hit.
setlocal
set H=c3891fa82c0a842db941d5d4656b35f69ecd45f6
set EXE=ds_gpu.exe
set LOG=logs
if not exist %LOG% mkdir %LOG%

echo [1/9] Vietnamese 2-word, no separator
%EXE% --hash %H% --wordfile wordlists\vie_words.txt --wordfile wordlists\vie_words.txt > %LOG%\1_vn2.log 2>&1
if %errorlevel%==0 goto found

echo [2/9] Vietnamese 2-word, space separator
%EXE% --hash %H% --wordfile wordlists\vie_words.txt --wordfile wordlists\vie_words.txt --sep " " > %LOG%\2_vn2s.log 2>&1
if %errorlevel%==0 goto found

echo [3/9] Vietnamese 3-word (common 3000)
%EXE% --hash %H% --wordfile wordlists\vie_common.txt --wordfile wordlists\vie_common.txt --wordfile wordlists\vie_common.txt --sep " " > %LOG%\3_vn3.log 2>&1
if %errorlevel%==0 goto found

echo [4/9] all printable ASCII, length 6
%EXE% --hash %H% --printable 6 > %LOG%\4_print6.log 2>&1
if %errorlevel%==0 goto found

echo [5/9] [a-z0-9]^7
%EXE% --hash %H% --mask abcdefghijklmnopqrstuvwxyz0123456789 7 > %LOG%\5_alnum7.log 2>&1
if %errorlevel%==0 goto found

echo [6/9] [a-z]^8
%EXE% --hash %H% --mask abcdefghijklmnopqrstuvwxyz 8 > %LOG%\6_lower8.log 2>&1
if %errorlevel%==0 goto found

echo [7/9] [a-z0-9]^8
%EXE% --hash %H% --mask abcdefghijklmnopqrstuvwxyz0123456789 8 > %LOG%\7_alnum8.log 2>&1
if %errorlevel%==0 goto found

echo [8/9] braces + mixed case, length 7
%EXE% --hash %H% --mask "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_{}" 7 > %LOG%\8_brace7.log 2>&1
if %errorlevel%==0 goto found

echo [9/9] all printable ASCII, length 7  (long: ~48h on a 3060)
%EXE% --hash %H% --printable 7 > %LOG%\9_print7.log 2>&1
if %errorlevel%==0 goto found

echo Nothing found in the whole list.
goto end

:found
echo.
echo ================= FOUND =================
type %LOG%\*.log | findstr /C:"MATCH"
echo Password is in the log above. Then run:
echo   python scripts\ds_extract.py "THE_PASSWORD" "Con chim non.wav"
echo =========================================
:end
endlocal
