[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Adb,
    [string]$Device
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $Adb)) {
    throw "ADB not found"
}

$available = @(
    & $Adb devices |
        Select-Object -Skip 1 |
        ForEach-Object {
            if ($_ -match '^([^\s]+)\s+device$') {
                $matches[1]
            }
        }
)

if (-not $Device) {
    if ($available.Count -ne 1) {
        throw "Expected exactly one online ADB device"
    }
    $Device = $available[0]
}

$moduleOutput = (
    & $Adb -s $Device shell "su -c '/data/adb/apd module list'" 2>&1
) -join "`n"

try {
    $modules = $moduleOutput | ConvertFrom-Json
} catch {
    throw "APatch module list is not valid JSON"
}

$module = $modules | Where-Object { $_.id -eq 'pftm10-control' } | Select-Object -First 1
if (-not $module) {
    throw "pftm10-control is not installed"
}
if ($module.version -ne '1.0.4' -or $module.versionCode -ne '5') {
    throw "Unexpected pftm10-control version"
}
if ($module.enabled -ne 'true' -or $module.update -ne 'false') {
    throw "pftm10-control is not in the applied and enabled state"
}

$health = (
    & $Adb -s $Device shell "su -c '/data/adb/modules/pftm10-control/bin/pftm10ctl health'" 2>&1
) -join "`n"
$caps = (
    & $Adb -s $Device shell "su -c '/data/adb/modules/pftm10-control/bin/pftm10ctl caps'" 2>&1
) -join "`n"

if ($health -notmatch '(?m)^health=ok$') {
    throw "Health check failed"
}
if ($caps -notmatch '(?m)^capabilities=ok$') {
    throw "Capability check failed"
}
if ($caps -notmatch '(?m)^kprobe_version=1\.1\.1$') {
    throw "Unexpected KPM version"
}
if ($caps -notmatch '(?m)^kprobe_event=post-fs-data$') {
    throw "Unexpected KPM load event"
}

Write-Output (
    "DEVICE_CHECK_OK module={0} versionCode={1} health=ok capabilities=ok kpm=1.1.1 event=post-fs-data" -f
        $module.version,
        $module.versionCode
)
