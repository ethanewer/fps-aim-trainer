Set-StrictMode -Version 3.0
$ErrorActionPreference = "Stop"

$Script:RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$Script:MsysRoot = if ($env:MSYS2_ROOT) { $env:MSYS2_ROOT } else { "C:\msys64" }
$Script:Bash = Join-Path $Script:MsysRoot "usr\bin\bash.exe"
$Script:UcrtBin = Join-Path $Script:MsysRoot "ucrt64\bin"
$Script:BuildDir = Join-Path $Script:RepoRoot "build"
$Script:ExePath = Join-Path $Script:BuildDir "aim-trainer.exe"

function Assert-WindowsToolchain {
    if (-not (Test-Path -LiteralPath $Script:Bash)) {
        throw "MSYS2 bash was not found at $Script:Bash. Install MSYS2 or set MSYS2_ROOT."
    }
    if (-not (Test-Path -LiteralPath (Join-Path $Script:UcrtBin "g++.exe"))) {
        throw "UCRT64 GCC was not found. Run: C:\msys64\usr\bin\bash.exe -lc `"pacman -S --needed --noconfirm mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-SDL2 mingw-w64-ucrt-x86_64-pkgconf make`""
    }
}

function ConvertTo-MsysPath([string]$Path) {
    $full = (Resolve-Path $Path).Path
    if ($full -match "^([A-Za-z]):\\(.*)$") {
        return "/" + $matches[1].ToLowerInvariant() + "/" + ($matches[2] -replace "\\", "/")
    }
    throw "Cannot convert path to MSYS form: $full"
}

function Invoke-Msys([string]$Command, [switch]$AllowFailure) {
    Assert-WindowsToolchain
    $repo = ConvertTo-MsysPath $Script:RepoRoot
    $wrapped = "cd '$repo' && export PATH=/ucrt64/bin:/usr/bin:`$PATH && $Command"
    & $Script:Bash -lc $wrapped
    $code = $LASTEXITCODE
    if (-not $AllowFailure -and $code -ne 0) {
        throw "MSYS command failed with exit code $code`: $Command"
    }
    if ($AllowFailure) {
        return $code
    }
}

function Sync-WindowsRuntimeDlls {
    New-Item -ItemType Directory -Force -Path $Script:BuildDir | Out-Null
    foreach ($dll in @("SDL2.dll", "libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll")) {
        $src = Join-Path $Script:UcrtBin $dll
        $dst = Join-Path $Script:BuildDir $dll
        if (-not (Test-Path -LiteralPath $src)) {
            throw "Missing runtime DLL: $src"
        }
        if (-not (Test-Path -LiteralPath $dst) -or (Get-Item -LiteralPath $src).LastWriteTimeUtc -gt (Get-Item -LiteralPath $dst).LastWriteTimeUtc) {
            Copy-Item -Force -LiteralPath $src -Destination $dst
        }
    }
}

function Test-WindowsBuildCurrent {
    if (-not (Test-Path -LiteralPath $Script:ExePath)) {
        return $false
    }
    $code = Invoke-Msys "make -q" -AllowFailure
    if ($code -eq 0) {
        return $true
    }
    if ($code -eq 1) {
        return $false
    }
    throw "make -q failed with exit code $code"
}

function Build-WindowsApp([switch]$Clean) {
    if ($Clean) {
        Invoke-Msys "make clean"
    }
    Invoke-Msys "make"
    Sync-WindowsRuntimeDlls
}
