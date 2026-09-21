param (
    [Parameter(Position=0)][string]$Password,
    [Parameter(Position=1)][string]$WavPath = "C:\Users\Admin\Downloads\Con chim non.wav",
    [string]$Wordlist,
    [string]$OutputDir = ".\deepsound_extracted",
    [switch]$Force
)

$ErrorActionPreference = "Stop"

$deepsoundDir = "C:\Program Files (x86)\DeepSound"
if (-not (Test-Path $deepsoundDir)) {
    Write-Error "DeepSound installation not found at $deepsoundDir"
    exit 1
}

# 1. Load official DeepSound assemblies
Add-Type -Path (Join-Path $deepsoundDir "Common.dll")
Add-Type -Path (Join-Path $deepsoundDir "Utils.dll")
Add-Type -Path (Join-Path $deepsoundDir "Steganography.dll")

Write-Host "[+] Loaded official DeepSound DLLs from $deepsoundDir" -ForegroundColor Cyan

if (-not (Test-Path $WavPath)) {
    Write-Error "WAV file not found: $WavPath"
    exit 1
}

$raw = [System.IO.File]::ReadAllBytes($WavPath)
Write-Host "[+] Loaded $WavPath ($($raw.Length) bytes)"

# 2. Locate the 'data' chunk in the WAV file
$dataTag = [System.Text.Encoding]::ASCII.GetBytes("data")
$baseOffset = -1

for ($i = 0; $i -lt $raw.Length - 8; $i++) {
    if ($raw[$i] -eq $dataTag[0] -and $raw[$i+1] -eq $dataTag[1] -and $raw[$i+2] -eq $dataTag[2] -and $raw[$i+3] -eq $dataTag[3]) {
        $chunkSize = [System.BitConverter]::ToUInt32($raw, $i + 4)
        if ($i + 8 + $chunkSize -le $raw.Length + 64 -and $chunkSize -gt 1000) {
            $baseOffset = $i + 8
            break
        }
    }
}

if ($baseOffset -lt 0) {
    Write-Error "Could not locate valid WAV audio 'data' chunk"
    exit 1
}
Write-Host "[+] Found audio 'data' payload at offset $baseOffset"

# 3. Decode 26-byte DeepSound container header
$headerBytes = New-Object byte[] 26
for ($i = 0; $i -lt 26; $i++) {
    $lowNibble1 = $raw[$baseOffset + $i * 4] -band 0x0F
    $lowNibble2 = $raw[$baseOffset + $i * 4 + 2] -band 0x0F
    $headerBytes[$i] = [byte](($lowNibble1 -shl 4) -bor $lowNibble2)
}

$magic = [System.Text.Encoding]::ASCII.GetString($headerBytes, 0, 4)
$mode = $headerBytes[4]
$encrypted = $headerBytes[5]
$targetKeyCheck = New-Object byte[] 20
[System.Array]::Copy($headerBytes, 6, $targetKeyCheck, 0, 20)
$targetHex = [System.BitConverter]::ToString($targetKeyCheck).Replace("-", "").ToLower()

Write-Host "================ DeepSound Container Header ================" -ForegroundColor Yellow
Write-Host "  Magic     : $magic"
Write-Host "  Quality   : Mode $mode (High=8, Normal=4, Low=2)"
Write-Host "  Encrypted : $($encrypted -eq 1)"
Write-Host "  Key-Check : $targetHex"
Write-Host "============================================================" -ForegroundColor Yellow

# 4. Determine Password (from parameter or wordlist scan)
$actualPassword = $null

if (-not [string]::IsNullOrEmpty($Wordlist)) {
    if (-not (Test-Path $Wordlist)) {
        Write-Error "Wordlist file not found: $Wordlist"
        exit 1
    }
    Write-Host "[*] Scanning wordlist with official DeepSound engine: $Wordlist" -ForegroundColor Cyan
    $csharpScanner = @'
using System;
using System.IO;
using System.Security.Cryptography;
using Jospin.Utils.Security;

public class DeepSoundFastScanner {
    public static string ScanWordlist(string path, byte[] target, out long tested) {
        tested = 0;
        using (var sha256 = SHA256.Create())
        using (var sha1 = SHA1.Create())
        using (var sr = new StreamReader(path)) {
            string line;
            while ((line = sr.ReadLine()) != null) {
                tested++;
                if (string.IsNullOrEmpty(line)) continue;
                byte[] key = sha256.ComputeHash(System.Text.Encoding.Unicode.GetBytes(line));
                byte[] encKey = AESUtils.EncryptData(key, key);
                byte[] check = sha1.ComputeHash(encKey);
                bool match = true;
                for (int i = 0; i < 20; i++) {
                    if (check[i] != target[i]) { match = false; break; }
                }
                if (match) return line;
            }
        }
        return null;
    }
}
'@
    Add-Type -TypeDefinition $csharpScanner -ReferencedAssemblies (Join-Path $deepsoundDir "Utils.dll")
    $testedCount = [long]0
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $actualPassword = [DeepSoundFastScanner]::ScanWordlist((Resolve-Path $Wordlist).Path, $targetKeyCheck, [ref]$testedCount)
    $sw.Stop()
    
    if ($actualPassword) {
        Write-Host "[+] MATCH FOUND IN WORDLIST! Password: '$actualPassword'" -ForegroundColor Green
        Write-Host "    (Checked $testedCount candidates in $($sw.Elapsed.TotalSeconds.ToString('F2')) s)"
    } else {
        Write-Host "[-] Checked $testedCount candidates in $($sw.Elapsed.TotalSeconds.ToString('F2')) s - NOT FOUND in $Wordlist" -ForegroundColor Red
        if (-not $Force) { exit 2 }
    }
} else {
    if ([string]::IsNullOrEmpty($Password)) {
        Write-Error "Please provide either -Password <string> or -Wordlist <path>"
        exit 1
    }
    $actualPassword = $Password
    
    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    $key = $sha256.ComputeHash([System.Text.Encoding]::Unicode.GetBytes($actualPassword))
    $sha1 = [System.Security.Cryptography.SHA1]::Create()
    $encKey = [Jospin.Utils.Security.AESUtils]::EncryptData($key, $key)
    $computedKeyCheck = $sha1.ComputeHash($encKey)
    $computedHex = [System.BitConverter]::ToString($computedKeyCheck).Replace("-", "").ToLower()

    Write-Host "[*] Provided Password : '$actualPassword'"
    Write-Host "[*] Derived Keycheck  : $computedHex"

    if ($computedHex -eq $targetHex) {
        Write-Host "[+] PASSWORD MATCH CONFIRMED!" -ForegroundColor Green
    } else {
        Write-Host "[-] Password does NOT match container key-check!" -ForegroundColor Red
        if (-not $Force) { exit 2 }
    }
}

# 5. Extract Steganographic Payload Bits (Mode 8)
Write-Host "[+] Extracting steganographic carrier bits (Mode $mode)..."
$payloadStart = $baseOffset + 104

$ms = New-Object System.IO.MemoryStream
if ($mode -eq 8) {
    for ($i = $payloadStart; $i -le $raw.Length - 8; $i += 8) {
        $b = [byte]((($raw[$i] -band 3) -shl 6) -bor (($raw[$i+2] -band 3) -shl 4) -bor (($raw[$i+4] -band 3) -shl 2) -bor ($raw[$i+6] -band 3))
        $ms.WriteByte($b)
    }
} elseif ($mode -eq 4) {
    for ($i = $payloadStart; $i -le $raw.Length - 4; $i += 4) {
        $b = [byte]((($raw[$i] -band 0x0F) -shl 4) -bor ($raw[$i+2] -band 0x0F))
        $ms.WriteByte($b)
    }
} elseif ($mode -eq 2) {
    for ($i = $payloadStart; $i -le $raw.Length - 2; $i += 2) {
        $ms.WriteByte($raw[$i])
    }
}
$cipherBytes = $ms.ToArray()
Write-Host "[+] Extracted $($cipherBytes.Length) payload bytes"

# 6. Decrypt payload with DeepSound AES-256-CBC
Write-Host "[+] Decrypting AES-256-CBC ciphertext..."
$sha256 = [System.Security.Cryptography.SHA256]::Create()
$key = $sha256.ComputeHash([System.Text.Encoding]::Unicode.GetBytes($actualPassword))
$iv = New-Object byte[] 16
[System.Array]::Copy($key, 0, $iv, 0, 16)

$aes = [System.Security.Cryptography.Aes]::Create()
$aes.Key = $key
$aes.IV = $iv
$aes.Mode = [System.Security.Cryptography.CipherMode]::CBC
$aes.Padding = [System.Security.Cryptography.PaddingMode]::None

$decryptor = $aes.CreateDecryptor()
$plain = $decryptor.TransformFinalBlock($cipherBytes, 0, ($cipherBytes.Length -band -16))

if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
}

# Check for DSSF header
$dssfTag = [System.Text.Encoding]::ASCII.GetBytes("DSSF")
$hasDSSF = ($plain[0] -eq $dssfTag[0] -and $plain[1] -eq $dssfTag[1] -and $plain[2] -eq $dssfTag[2] -and $plain[3] -eq $dssfTag[3])

if ($hasDSSF) {
    Write-Host "[+] Found DeepSound Secret File (DSSF) header!" -ForegroundColor Green
    $pos = 0
    $fileCount = 0
    while ($pos + 28 -le $plain.Length) {
        if ($plain[$pos] -ne $dssfTag[0] -or $plain[$pos+1] -ne $dssfTag[1]) { break }
        $nameBytes = New-Object byte[] 20
        [System.Array]::Copy($plain, $pos + 4, $nameBytes, 0, 20)
        $name = [System.Text.Encoding]::UTF8.GetString($nameBytes).Trim([char]0, ' ')
        
        $lenBytes = @($plain[$pos+27], $plain[$pos+26], $plain[$pos+25], $plain[$pos+24])
        $fileLen = [System.BitConverter]::ToUInt32($lenBytes, 0)
        $pos += 32
        
        if ($pos + $fileLen -le $plain.Length) {
            $fileData = New-Object byte[] $fileLen
            [System.Array]::Copy($plain, $pos, $fileData, 0, $fileLen)
            $outPath = Join-Path $OutputDir $name
            [System.IO.File]::WriteAllBytes($outPath, $fileData)
            Write-Host "    --> Extracted: $outPath ($fileLen bytes)" -ForegroundColor Green
            $fileCount++
            $pos += $fileLen
            if ($pos % 16 -ne 0) { $pos += (16 - ($pos % 16)) }
        } else {
            break
        }
    }
    Write-Host "[+] Successfully extracted $fileCount file(s) into $OutputDir" -ForegroundColor Green
} else {
    Write-Host "[-] First block does not start with DSSF signature" -ForegroundColor Yellow
    $rawPayloadPath = Join-Path $OutputDir "decrypted_payload.bin"
    [System.IO.File]::WriteAllBytes($rawPayloadPath, $plain)
    Write-Host "[*] Dumped raw decrypted stream ($($plain.Length) bytes) to: $rawPayloadPath"
}
