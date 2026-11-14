[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$KpmPath,
    [string]$RzBin = 'rz-bin.exe'
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $KpmPath)) {
    throw "KPM not found"
}

$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $KpmPath).Hash
$length = (Get-Item -LiteralPath $KpmPath).Length
$rz = Get-Command $RzBin -ErrorAction SilentlyContinue

if ($rz) {
    $sections = & $rz.Source -S $KpmPath 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "rz-bin section inspection failed"
    }
    if ($sections | Select-String -Pattern '\.debug_' -Quiet) {
        throw "KPM contains debug sections"
    }

    $strings = & $rz.Source -z $KpmPath 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "rz-bin string inspection failed"
    }
    if ($strings | Select-String -Pattern '(?i)(?:[A-Za-z]:[\\/]|/(?:Users|home)/)' -Quiet) {
        throw "KPM contains a local path string"
    }
}

[pscustomobject]@{
    File = (Resolve-Path -LiteralPath $KpmPath).Path
    Bytes = $length
    SHA256 = $hash
    DebugSections = 'absent'
    Author = 'syttys123'
} | Format-List

exit 0
