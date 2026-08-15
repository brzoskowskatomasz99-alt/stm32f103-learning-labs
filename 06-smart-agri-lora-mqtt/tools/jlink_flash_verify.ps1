<#
jlink_flash_verify.ps1 - J-Link flash + verify with independent readback
(P1 plugin core, ASCII-only source)

Usage:
  # flash and verify (loadbin + verifybin + mem32 readback SHA256 compare)
  .\tools\jlink_flash_verify.ps1 -Bin .\MDK-ARM\Template_GATEWAY_M3.bin

  # readback-only compare (does NOT touch the firmware on target)
  .\tools\jlink_flash_verify.ps1 -Bin .\MDK-ARM\Template_GATEWAY_M3.bin -ReadBackOnly

  # dry run (build the CommanderScript only, no hardware access)
  .\tools\jlink_flash_verify.ps1 -Bin .\MDK-ARM\Template_GATEWAY_M3.bin -DryRun

Params:
  -Bin           firmware bin path
  -JLink         JLink.exe path (default C:\Keil_v5\ARM\Segger\JLink.exe)
  -Device        target device (default STM32F103C8)
  -Speed         SWD speed kHz (default 4000)
  -Addr          flash address (default 0x08000000)
  -KillStale     kill leftover JLink.exe processes first (default ON, prevents Out of sync)
  -ReadBackOnly  readback compare only, no loadbin/verifybin
  -DryRun        generate CommanderScript only
Exit: 0 pass, 1 fail
#>
param(
    [string]$Bin = "",
    [string]$JLink = "C:\Keil_v5\ARM\Segger\JLink.exe",
    [string]$Device = "STM32F103C8",
    [int]$Speed = 4000,
    [string]$Addr = "0x08000000",
    [switch]$KillStale = $true,
    [switch]$ReadBackOnly,
    [switch]$DryRun,
    [switch]$SelfTest
)

$ErrorActionPreference = "Continue"
$PSNativeCommandUseErrorActionPreference = $false

function Get-Sha256Hex([string]$Path) {
    return (Get-FileHash -Algorithm SHA256 -Path $Path).Hash
}

if (-not $Bin) { Write-Output "[JLINK] -Bin is required"; exit 1 }
if (-not (Test-Path $Bin)) { Write-Output "[JLINK] BIN NOT FOUND: $Bin"; exit 1 }
if (-not (Test-Path $JLink)) { Write-Output "[JLINK] JLink.exe NOT FOUND: $JLink (use -JLink)"; exit 1 }

$binSize = (Get-Item $Bin).Length
$binSha = Get-Sha256Hex $Bin
$words = [math]::Ceiling($binSize / 4)
Write-Output ("[JLINK] BIN={0} SIZE={1} SHA256={2}" -f $Bin, $binSize, $binSha)

if ($SelfTest) {
    # 离线自检: 用 bin 本身构造合成 mem32 输出, 走与真机完全相同的解析代码,
    # 验证正则提取/端序/截断/SHA 比对逻辑(不接触硬件)
    $bytes = [IO.File]::ReadAllBytes((Resolve-Path $Bin))
    $sb = New-Object System.Text.StringBuilder
    for ($i = 0; $i -lt $words; $i++) {
        $off = $i * 4
        $v = 0
        for ($b = 0; $b -lt 4; $b++) {
            $byteVal = if (($off + $b) -lt $binSize) { $bytes[$off + $b] } else { 0 }
            $v = $v -bor ([int]$byteVal -shl (8 * $b))
        }
        $addr = "{0:X8}" -f (0x08000000 + $off)
        $null = $sb.AppendFormat("{0} = {1:X8}{2}", $addr, $v,
                                 $(if (($i % 4) -eq 3) { "`n" } else { "  " }))
    }
    $out = $sb.ToString()
} else {

$scriptLines = @(
    "device $Device",
    "si 1",
    "speed $Speed"
)
if (-not $ReadBackOnly) {
    $scriptLines += "loadbin `"$Bin`" $Addr"
    $scriptLines += "verifybin `"$Bin`" $Addr"
}
$scriptLines += "mem32 $Addr $words"
$scriptLines += "q"

$tmpDir = Join-Path $env:TEMP "jlink_flash_verify"
New-Item -ItemType Directory -Path $tmpDir -Force | Out-Null
$scriptPath = Join-Path $tmpDir "flash_verify.jlink"
Set-Content -Path $scriptPath -Value $scriptLines -Encoding ASCII
$outPath = Join-Path $tmpDir "jlink_out.txt"

if ($DryRun) {
    Write-Output "[JLINK] DRY-RUN CommanderScript:"
    Get-Content $scriptPath
    exit 0
}

if ($KillStale) {
    $stale = Get-Process JLink -ErrorAction SilentlyContinue
    if ($stale) {
        Write-Output ("[JLINK] killing stale JLink.exe x{0}" -f $stale.Count)
        $stale | Stop-Process -Force
        Start-Sleep -Milliseconds 500
    }
}

& $JLink -CommanderScript $scriptPath *> $outPath
$out = Get-Content $outPath -Raw -ErrorAction SilentlyContinue
}

$verifyOk = ($out -match "Verify successful")
$memWords = @()
[regex]::Matches($out, '[0-9A-Fa-f]{8}\s*=\s*([0-9A-Fa-f]{8})') |
    ForEach-Object { $memWords += $_.Groups[1].Value }
$readbackOk = $false
if ($memWords.Count -ge $words) {
    $ms = New-Object System.IO.MemoryStream
    foreach ($w in $memWords[0..($words - 1)]) {
        $v = [Convert]::ToUInt32($w, 16)
        $ms.WriteByte($v -band 0xFF)
        $ms.WriteByte(($v -shr 8) -band 0xFF)
        $ms.WriteByte(($v -shr 16) -band 0xFF)
        $ms.WriteByte(($v -shr 24) -band 0xFF)
    }
    $bytes = $ms.ToArray()
    $ms.Dispose()
    if ($bytes.Length -ge $binSize) {
        $trimmed = New-Object byte[] $binSize
        [Array]::Copy($bytes, $trimmed, $binSize)
        $sha = [System.Security.Cryptography.SHA256]::Create()
        $readSha = ($sha.ComputeHash($trimmed) |
                    ForEach-Object { $_.ToString("X2") }) -join ""
        $readbackOk = ($readSha -eq $binSha)
        Write-Output ("[JLINK] READBACK SHA256={0} MATCH={1}" -f $readSha, $readbackOk)
    }
} else {
    Write-Output ("[JLINK] READBACK FAIL: only {0}/{1} words read" -f $memWords.Count, $words)
}

if (-not $ReadBackOnly -and -not $SelfTest -and -not $verifyOk) {
    Write-Output "[JLINK] VERIFYBIN FAIL"
    if ($out) { Write-Output ($out -split "`n" | Select-Object -Last 8) }
    exit 1
}
if (-not $readbackOk) {
    Write-Output "[JLINK] independent readback FAIL (target not connected?)"
    exit 1
}
Write-Output "[JLINK] PASS"
exit 0
