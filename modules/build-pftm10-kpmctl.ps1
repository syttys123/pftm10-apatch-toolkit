[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Zig,
    [Parameter(Mandatory = $true)]
    [string]$OutputPath,
    [string]$KpmName = 'pftm10-kprobe'
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $Zig)) {
    throw "Zig compiler not found"
}

$source = Join-Path $PSScriptRoot 'src\pftm10-kpmctl.c'
$outputDirectory = Split-Path -Parent $OutputPath
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null

& $Zig cc `
    -target aarch64-linux-musl `
    -Os `
    -static `
    -fno-pie `
    "-DKPM_NAME=`"$KpmName`"" `
    '-Wl,-z,noexecstack' `
    -o $OutputPath `
    $source

if ($LASTEXITCODE -ne 0) {
    throw "KPM control helper build failed"
}

$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $OutputPath).Hash

[pscustomobject]@{
    File = $OutputPath
    Bytes = (Get-Item -LiteralPath $OutputPath).Length
    SHA256 = $hash
} | Format-List

exit 0
