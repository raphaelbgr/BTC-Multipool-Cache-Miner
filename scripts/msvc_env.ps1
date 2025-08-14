# Set MSVC and Windows SDK environment variables for building with Ninja/cl on Windows

# Prefer VS Dev env variables if present; otherwise fall back to standard install paths
$msvcRootEnv = $env:VCToolsInstallDir
if (-not $msvcRootEnv -or -not (Test-Path $msvcRootEnv)) {
    $msvcRootEnv = 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\'
}
$msvcRoot = $msvcRootEnv.TrimEnd('\')
$ccbin    = Join-Path $msvcRoot 'bin\Hostx64\x64'

$sdkDir = $env:WindowsSdkDir
$sdkVer = $env:WindowsSDKVersion
if (-not $sdkDir -or -not $sdkVer) {
    $sdkDir = 'C:\Program Files (x86)\Windows Kits\10\'
    $sdkVer = '10.0.26100.0\'
}
$sdkBase = Split-Path -Parent $sdkDir
$sdkVer  = $sdkVer.TrimEnd('\')

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

Write-Host "MSVC env set:" -ForegroundColor Green
Write-Host "  cl.exe:    $ccbin\cl.exe"
Write-Host "  INCLUDE:   $env:INCLUDE"
Write-Host "  LIB:       $env:LIB"

