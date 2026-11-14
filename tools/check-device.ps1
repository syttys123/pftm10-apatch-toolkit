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
if ($module.version -ne '1.2.1' -or $module.versionCode -ne '8') {
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
if ($caps -notmatch '(?m)^kprobe_control=ok$') {
    throw "KPM control0 check failed"
}
if ($caps -notmatch '(?m)^battery_restrict=masked$') {
    throw "Battery restrict crash mask is not active"
}

$cameraFine = (
    & $Adb -s $Device shell 'cmd appops get com.oplus.camera FINE_LOCATION' 2>&1
) -join "`n"
$cameraCoarse = (
    & $Adb -s $Device shell 'cmd appops get com.oplus.camera COARSE_LOCATION' 2>&1
) -join "`n"
if ($cameraFine -notmatch '(?m)^FINE_LOCATION:\s+ignore;') {
    throw "Camera fine-location AppOp is not denied"
}
if ($cameraCoarse -notmatch '(?m)^COARSE_LOCATION:\s+ignore;') {
    throw "Camera coarse-location AppOp is not denied"
}

$oshare = (
    & $Adb -s $Device shell 'dumpsys package com.coloros.oshare' 2>&1
) -join " "
$oshare = $oshare -replace '\s+', ' '
if ($oshare -notmatch 'ACCESS_BACKGROUND_LOCATION: granted=false') {
    throw "OShare background location is not denied"
}
if ($oshare -notmatch 'ACCESS_BACKGROUND_LOCATION: granted=false, flags=\[[^\]]*USER_FIXED') {
    throw "OShare background location is not fixed"
}

$controlHelp = (
    & $Adb -s $Device shell "su -c '/data/adb/modules/pftm10-control/bin/pftm10ctl kprobe-ctl help'" 2>&1
) -join "`n"
if ($controlHelp -notmatch 'commands: info \| banner \| cmdline') {
    throw "KPM control0 help response is invalid"
}

Write-Output (
    "DEVICE_CHECK_OK module={0} versionCode={1} health=ok capabilities=ok kpm=1.1.1 control=ok battery_restrict=masked privacy=camera_location_denied,oshare_bg_denied event=post-fs-data" -f
        $module.version,
        $module.versionCode
)
