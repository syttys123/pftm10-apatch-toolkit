[CmdletBinding()]
param(
    [string]$Zig = 'E:\AI\tools\zig-0.14.1\zig-x86_64-windows-0.14.1\zig.exe',
    [string]$KernelPatchRoot = $env:PFTM10_KERNELPATCH_ROOT
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $Zig)) {
    throw "Zig compiler not found: $Zig"
}
if (-not $KernelPatchRoot) {
    throw "Set PFTM10_KERNELPATCH_ROOT to the local KernelPatch source directory"
}
if (-not (Test-Path -LiteralPath $KernelPatchRoot)) {
    throw "KernelPatch source root not found: $KernelPatchRoot"
}

$sourceRoot = $PSScriptRoot
$obj = Join-Path $sourceRoot 'pftm10-kprobe.o'
$kpm = Join-Path $sourceRoot 'pftm10-kprobe.kpm'
$source = Join-Path $sourceRoot 'pftm10-kprobe.c'
$linkerScript = Join-Path $sourceRoot 'pftm10-kprobe.lds'

$includeRoots = @(
    '.',
    'include',
    'patch/include',
    'linux/include',
    'linux/arch/arm64/include',
    'linux/tools/arch/arm64/include'
) | ForEach-Object {
    '-I' + (Join-Path $KernelPatchRoot ('kernel\' + $_))
}

$commonFlags = @(
    '-target', 'aarch64-freestanding',
    '-nostdinc',
    '-fno-pic',
    '-fno-PIC',
    '-fno-pie',
    '-fno-stack-protector',
    '-fno-asynchronous-unwind-tables',
    '-fno-unwind-tables',
    '-fno-exceptions',
    '-g0'
)

& $Zig cc @commonFlags @includeRoots "-T$linkerScript" -c -O2 -o $obj $source
if ($LASTEXITCODE -ne 0) {
    throw "KPM compile failed with exit code $LASTEXITCODE"
}

& $Zig cc -target aarch64-freestanding -r -nostdlib -o $kpm $obj
if ($LASTEXITCODE -ne 0) {
    throw "KPM link failed with exit code $LASTEXITCODE"
}

$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $kpm).Hash
$hash | Set-Content -LiteralPath ($kpm + '.sha256') -Encoding ascii

[pscustomobject]@{
    Kpm = $kpm
    Bytes = (Get-Item -LiteralPath $kpm).Length
    SHA256 = $hash
} | Format-List
