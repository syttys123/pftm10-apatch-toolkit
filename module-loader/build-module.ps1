[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ModulesPath,
    [string]$Stamp = (Get-Date -Format 'yyyyMMdd-HHmmss')
)

$ErrorActionPreference = 'Stop'

$root = $PSScriptRoot
$source = Join-Path $root 'src'
$dist = Join-Path $root 'dist'
$stage = Join-Path $dist ('stage-' + $Stamp)

$modulePropPath = Join-Path $source 'module.prop'
if (-not (Test-Path -LiteralPath $modulePropPath)) {
    throw "Missing module source: $modulePropPath"
}

$version = $null
foreach ($line in Get-Content -LiteralPath $modulePropPath) {
    if ($line -match '^version=(.+)$') {
        $version = $Matches[1].Trim()
        break
    }
}
if (-not $version) {
    throw 'module.prop does not contain a version'
}

$moduleNames = @(
    'trace_mmstat.ko',
    'connfem.ko',
    'wmt_drv.ko',
    'wmt_chrdev_wifi.ko',
    'bt_drv_connac1x.ko',
    'wlan_drv_gen4m.ko',
    'gps_drv.ko',
    'fmradio_drv_mt6631_6635.ko',
    'fpsgo.ko',
    'met.ko'
)

if (-not (Test-Path -LiteralPath $ModulesPath -PathType Container)) {
    throw "ModulesPath is not a directory: $ModulesPath"
}

$missing = @(
    $moduleNames | Where-Object {
        -not (Test-Path -LiteralPath (Join-Path $ModulesPath $_))
    }
)
if ($missing.Count -gt 0) {
    throw ('Missing rebuilt modules: ' + ($missing -join ', '))
}

New-Item -ItemType Directory -Path $stage -Force | Out-Null
Copy-Item -Path (Join-Path $source '*') -Destination $stage -Recurse -Force

$stageModules = Join-Path $stage 'modules/4.19.191+'
New-Item -ItemType Directory -Path $stageModules -Force | Out-Null

$manifest = foreach ($name in $moduleNames) {
    $inputPath = Join-Path $ModulesPath $name
    $outputPath = Join-Path $stageModules $name
    Copy-Item -LiteralPath $inputPath -Destination $outputPath -Force
    $hash = Get-FileHash -Algorithm SHA256 -LiteralPath $outputPath
    [pscustomobject]@{
        path = "modules/4.19.191+/$name"
        bytes = (Get-Item -LiteralPath $outputPath).Length
        sha256 = $hash.Hash
    }
}
$manifest | Export-Csv -LiteralPath (Join-Path $stage 'module-manifest.csv') `
    -NoTypeInformation -Encoding Ascii

$zip = Join-Path $dist (
    'pftm10-kernel-modules-v' + $version + '-' + $Stamp + '.zip'
)

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

if (Test-Path -LiteralPath $zip) {
    Remove-Item -LiteralPath $zip -Force
}

$archive = [System.IO.Compression.ZipFile]::Open(
    $zip,
    [System.IO.Compression.ZipArchiveMode]::Create
)

try {
    Get-ChildItem -LiteralPath $stage -Recurse -File | ForEach-Object {
        $relative = $_.FullName.Substring($stage.Length + 1).Replace('\', '/')
        $entry = $archive.CreateEntry(
            $relative,
            [System.IO.Compression.CompressionLevel]::Optimal
        )
        $entry.LastWriteTime = $_.LastWriteTime

        $mode = if ($relative.EndsWith('.sh')) { 0x81ED } else { 0x81A4 }
        $attributeBits = [uint32]([uint64]$mode * 65536)
        $entry.ExternalAttributes = [BitConverter]::ToInt32(
            [BitConverter]::GetBytes($attributeBits),
            0
        )

        $input = [System.IO.File]::OpenRead($_.FullName)
        $output = $entry.Open()
        try {
            $input.CopyTo($output)
        }
        finally {
            $output.Dispose()
            $input.Dispose()
        }
    }
}
finally {
    $archive.Dispose()
}

# Android unzip applies Unix external attributes only when the central
# directory identifies Unix as the creating host.
$bytes = [System.IO.File]::ReadAllBytes($zip)
$centralHeaders = 0
for ($i = 0; $i -le $bytes.Length - 46; $i++) {
    if (
        $bytes[$i] -eq 0x50 -and
        $bytes[$i + 1] -eq 0x4B -and
        $bytes[$i + 2] -eq 0x01 -and
        $bytes[$i + 3] -eq 0x02
    ) {
        $bytes[$i + 5] = 3
        $centralHeaders++
    }
}
if ($centralHeaders -eq 0) {
    throw 'No ZIP central-directory headers were found'
}
[System.IO.File]::WriteAllBytes($zip, $bytes)

$hash = Get-FileHash -Algorithm SHA256 -LiteralPath $zip
$item = Get-Item -LiteralPath $zip
$checksum = $zip + '.sha256'
Set-Content -LiteralPath $checksum -Value (
    $hash.Hash.ToLowerInvariant() + '  ' + $item.Name
) -Encoding Ascii

Write-Output ('ZIP=' + $zip)
Write-Output ('BYTES=' + $item.Length)
Write-Output ('SHA256=' + $hash.Hash)
Write-Output ('STAGE=' + $stage)
