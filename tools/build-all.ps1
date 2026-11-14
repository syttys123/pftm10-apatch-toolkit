[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Zig,
    [Parameter(Mandatory = $true)]
    [string]$KernelPatchRoot,
    [string]$OutputDirectory = (Join-Path (Split-Path -Parent $PSScriptRoot) 'artifacts')
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$outputRoot = [IO.Path]::GetFullPath($OutputDirectory)

New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null

$env:PFTM10_KERNELPATCH_ROOT = $KernelPatchRoot

& (Join-Path $PSScriptRoot 'validate-public-tree.ps1')
if ($LASTEXITCODE -ne 0) {
    throw "Public tree validation failed"
}

& (Join-Path $repoRoot 'modules\build-pftm10-control.ps1') -OutputDirectory $outputRoot
if ($LASTEXITCODE -ne 0) {
    throw "System module build failed"
}

& (Join-Path $repoRoot 'kpms\pftm10-kprobe\build.ps1') -Zig $Zig -KernelPatchRoot $KernelPatchRoot
if ($LASTEXITCODE -ne 0) {
    throw "KPM build failed"
}

$kpmSource = Join-Path $repoRoot 'kpms\pftm10-kprobe\pftm10-kprobe.kpm'
$kpmDestination = Join-Path $outputRoot 'pftm10-kprobe.kpm'
Copy-Item -LiteralPath $kpmSource -Destination $kpmDestination -Force

& (Join-Path $PSScriptRoot 'verify-kpm.ps1') -KpmPath $kpmDestination
if ($LASTEXITCODE -ne 0) {
    throw "KPM verification failed"
}

$moduleProp = Get-Content -LiteralPath (Join-Path $repoRoot 'modules\pftm10-control\module.prop')
$moduleVersionLine = $moduleProp |
    Where-Object { $_ -match '^version=' } |
    Select-Object -First 1
if (-not $moduleVersionLine) {
    throw "Cannot determine current module version"
}
$moduleVersion = ($moduleVersionLine -split '=', 2)[1].Trim()
$expectedModuleZip = "pftm10-control-v$moduleVersion.zip"

$artifacts = Get-ChildItem -LiteralPath $outputRoot -File |
    Where-Object { $_.Name -in @($expectedModuleZip, 'pftm10-kprobe.kpm') } |
    ForEach-Object {
        [ordered]@{
            name = $_.Name
            bytes = $_.Length
            sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash
            author = 'syttys123'
        }
    }

$manifest = [ordered]@{
    release = 'v1.1.0'
    builder = 'syttys123'
    artifacts = $artifacts
}

$manifest |
    ConvertTo-Json -Depth 4 |
    Set-Content -LiteralPath (Join-Path $outputRoot 'artifacts.json') -Encoding utf8

Write-Output "BUILD_ALL_OK output=$outputRoot"
