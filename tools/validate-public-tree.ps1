[CmdletBinding()]
param(
    [string]$Root = (Split-Path -Parent $PSScriptRoot),
    [string[]]$DenyLiteral = @()
)

$ErrorActionPreference = 'Stop'
$resolvedRoot = [IO.Path]::GetFullPath($Root)

$legacyAssistant = 'Co' + 'dex'
$privateStateName = 'PFTM10' + '-ROOT-' + 'STATE'

$patterns = @(
    "Documents[\\/]$legacyAssistant",
    "[.]$($legacyAssistant.ToLower())[\\/]",
    $privateStateName,
    'ADB_SERIAL',
    "author\s*=\s*$legacyAssistant",
    ('KPM_AUTHOR\("{0}"\)' -f $legacyAssistant)
)

$files = Get-ChildItem -LiteralPath $resolvedRoot -Recurse -File |
    Where-Object {
        $_.FullName -notmatch '[\\/]\.git[\\/]' -and
        $_.FullName -ne $PSCommandPath -and
        $_.Extension -notin @('.zip', '.kpm', '.png', '.jpg', '.jpeg', '.webp')
    }

$failures = [System.Collections.Generic.List[string]]::new()

foreach ($pattern in $patterns) {
    foreach ($match in $files | Select-String -Pattern $pattern -ErrorAction SilentlyContinue) {
        $failures.Add("$($match.Path):$($match.LineNumber): $pattern")
    }
}

foreach ($literal in $DenyLiteral) {
    if (-not $literal) {
        continue
    }
    foreach ($match in $files | Select-String -SimpleMatch -Pattern $literal -ErrorAction SilentlyContinue) {
        $failures.Add("$($match.Path):$($match.LineNumber): literal")
    }
}

if ($failures.Count -gt 0) {
    $failures | ForEach-Object { Write-Error $_ }
    exit 1
}

Write-Output "PUBLIC_TREE_OK files=$($files.Count)"
