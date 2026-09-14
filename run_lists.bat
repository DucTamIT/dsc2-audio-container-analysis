@echo off
REM Run each list in turn; stop at the first match (exit code 0).
setlocal enabledelayedexpansion
set H=c3891fa82c0a842db941d5d4656b35f69ecd45f6
set EXE=ds_gpu.exe
if not exist logs mkdir logs
for %%F in (rockyou2021.txt rockyou2024.txt xato-net-10-million-passwords.txt crackstation-human-only.txt kaonashi.txt rockyou-withcount.txt) do (
  if exist "%%F" (
    echo === %%F ===
    %EXE% --hash %H% --wordlist "%%F" --batch 33554432 > "logs\%%F.log" 2>&1
    if !errorlevel! == 0 (
      echo.
      echo *** FOUND in %%F ***
      findstr /C:"MATCH" "logs\%%F.log"
      exit /b 0
    )
  ) else (
    echo skipping %%F ^(not present^)
  )
)
echo Nothing found in the lists that were present.
endlocal
