# Auto-detect MSVC and Windows SDK include/lib paths and export for the current PowerShell session

function Get-LatestSdkVersion {
    param([string]$Base = 'C:\Program Files (x86)\Windows Kits\10\Include')
    if (!(Test-Path $Base)) { return $null }
    $dirs = Get-ChildItem -Path $Base -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match '^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$' } |
        Sort-Object { [version]$_.Name } -Descending
    if ($dirs -and $dirs.Count -gt 0) { return $dirs[0].Name }
    return $null
}

$msvcRoot = $env:VCToolsInstallDir
if (-not $msvcRoot -or -not (Test-Path $msvcRoot)) {
    $msvcRoot = 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\'
}
$msvcRoot = $msvcRoot.TrimEnd('\')
$ccbin    = Join-Path $msvcRoot 'bin\Hostx64\x64'

$sdkBase = 'C:\Program Files (x86)\Windows Kits\10'
$sdkVer  = Get-LatestSdkVersion -Base (Join-Path $sdkBase 'Include')
if (-not $sdkVer) { Write-Warning 'Windows 10/11 SDK not found under Windows Kits\10\Include'; return }

$include = @(
    "$msvcRoot\include",
    "$sdkBase\Include\$sdkVer\ucrt",
    "$sdkBase\Include\$sdkVer\um",
    "$sdkBase\Include\$sdkVer\shared",
    "$sdkBase\Include\$sdkVer\winrt",
    "$sdkBase\Include\$sdkVer\cppwinrt"
) -join ';'

$lib = @(
    "$msvcRoot\lib\x64",
    "$sdkBase\Lib\$sdkVer\ucrt\x64",
    "$sdkBase\Lib\$sdkVer\um\x64"
) -join ';'

$pathPrepend = @(
    $ccbin,
    "$sdkBase\bin\$sdkVer\x64"
) -join ';'

$env:INCLUDE = $include
$env:LIB     = $lib
$env:PATH    = "$pathPrepend;$($env:PATH)"

Write-Host "MSVC env set (auto):" -ForegroundColor Green
Write-Host "  SDK ver:   $sdkVer"
Write-Host "  cl.exe:    $ccbin\cl.exe"
Write-Host "  INCLUDE ucrt exists: " (Test-Path "$sdkBase\Include\$sdkVer\ucrt\stddef.h")

