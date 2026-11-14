[CmdletBinding()]
param(
    [string]$OutputDirectory = (Join-Path $PSScriptRoot 'dist')
)

$ErrorActionPreference = 'Stop'

$moduleSource = Join-Path $PSScriptRoot 'pftm10-control'
$outputRoot = [IO.Path]::GetFullPath($OutputDirectory)
$stage = [IO.Path]::GetFullPath((Join-Path $outputRoot 'stage'))
$moduleProp = Get-Content -LiteralPath (Join-Path $moduleSource 'module.prop')
$versionLine = $moduleProp | Where-Object { $_ -match '^version=' } | Select-Object -First 1
if (-not $versionLine) {
    throw "module.prop does not contain a version field"
}
$version = ($versionLine -split '=', 2)[1].Trim()
$zipPath = [IO.Path]::GetFullPath((Join-Path $outputRoot ("pftm10-control-v$version.zip")))

if (Test-Path -LiteralPath $stage) {
    $resolvedStage = [IO.Path]::GetFullPath($stage)
    if (-not $resolvedStage.StartsWith($outputRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove unexpected stage path: $resolvedStage"
    }
    Remove-Item -LiteralPath $resolvedStage -Recurse -Force
}

New-Item -ItemType Directory -Path $stage -Force | Out-Null
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null

Get-ChildItem -LiteralPath $moduleSource -Recurse -File | ForEach-Object {
    $relative = [IO.Path]::GetRelativePath($moduleSource, $_.FullName)
    $destination = Join-Path $stage $relative
    $destinationDirectory = Split-Path -Parent $destination
    New-Item -ItemType Directory -Path $destinationDirectory -Force | Out-Null
    Copy-Item -LiteralPath $_.FullName -Destination $destination -Force
}

if (Test-Path -LiteralPath $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}

[IO.Compression.ZipFile]::CreateFromDirectory(
    $stage,
    $zipPath,
    [IO.Compression.CompressionLevel]::Optimal,
    $false
)

$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $zipPath).Hash
$size = (Get-Item -LiteralPath $zipPath).Length
$hash | Set-Content -LiteralPath ($zipPath + '.sha256') -Encoding ascii

[pscustomobject]@{
    Zip = $zipPath
    Bytes = $size
    SHA256 = $hash
} | Format-List
