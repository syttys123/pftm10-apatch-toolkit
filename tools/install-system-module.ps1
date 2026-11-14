[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Adb,
    [Parameter(Mandatory = $true)]
    [string]$ModuleZip,
    [Parameter(Mandatory = $true)]
    [string]$Device
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $Adb)) {
    throw "ADB not found"
}
if (-not (Test-Path -LiteralPath $ModuleZip)) {
    throw "Module ZIP not found"
}

$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $ModuleZip).Hash
$remote = "/data/local/tmp/" + (Split-Path -Leaf $ModuleZip)

& $Adb -s $Device push $ModuleZip $remote
if ($LASTEXITCODE -ne 0) {
    throw "ADB push failed"
}

& $Adb -s $Device shell "sha256sum $remote"
if ($LASTEXITCODE -ne 0) {
    throw "Remote hash read failed"
}

& $Adb -s $Device shell "su -c '/data/adb/apd module install $remote'"
if ($LASTEXITCODE -ne 0) {
    throw "APatch module install failed"
}

Write-Output "LOCAL_SHA256=$hash"
Write-Output "REBOOT_REQUIRED=true"
