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
if ($module.version -ne '1.5.7' -or $module.versionCode -ne '18') {
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
if ($caps -notmatch '(?m)^kprobe_version=1\.4\.6$') {
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

$cpuMaxDeadline = (Get-Date).AddSeconds(60)
do {
    $cpuMax = (
        & $Adb -s $Device shell "su -c '/data/adb/modules/pftm10-control/bin/pftm10ctl cpu-max status'" 2>&1
    ) -join "`n"
    $cpuMaxReady = (
        $cpuMax -match '(?m)^cpu_max=disabled$' -or
        ($cpuMax -match '(?m)^cpu_max=armed$' -and
         $cpuMax -match '(?m)^cpu_max_kpm=loaded$' -and
         $cpuMax -match '(?m)^cpu_max_watchdog=running$')
    )
    if (-not $cpuMaxReady) {
        Start-Sleep -Seconds 2
    }
} while (-not $cpuMaxReady -and (Get-Date) -lt $cpuMaxDeadline)
if ($cpuMax -notmatch '(?m)^cpu_max=(disabled|armed)$') {
    throw "CPU max policy state is invalid"
}
$cpuMaxMode = $matches[1]
if ($cpuMaxMode -eq 'disabled') {
    if ($cpuMax -notmatch '(?m)^cpu_max_kpm=unloaded$' -or
        $cpuMax -notmatch '(?m)^cpu_max_watchdog=missing$') {
        throw "Disabled CPU max policy left a loaded KPM or watchdog"
    }
} else {
    if ($cpuMax -notmatch '(?m)^cpu_max_kpm=loaded$' -or
        $cpuMax -notmatch '(?m)^cpu_max_watchdog=running$' -or
        $cpuMax -notmatch '(?m)^cpu_max_target_khz=2400000$') {
        throw "Armed CPU max policy is incomplete"
    }
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
if ($controlHelp -notmatch 'charger-unsuspend') {
    throw "KPM charger recovery command is unavailable"
}
if ($controlHelp -notmatch 'pd-contract' -or
    $controlHelp -notmatch 'charger-3a' -or
    $controlHelp -notmatch 'charger-restore') {
    throw "KPM gated 3A charger controls are unavailable"
}
if ($controlHelp -notmatch 'soc-reconcile') {
    throw "KPM SOC reconciliation control is unavailable"
}

$bootLog = (
    & $Adb -s $Device shell 'su -c "cat /data/adb/pftm10-control/boot.log"' 2>&1
) -join "`n"
if ($bootLog -notmatch '(?m)^charger_unsuspend_rc=0$' -or
    $bootLog -notmatch 'charger_unsuspend_called=1') {
    throw "Boot-time charger suspend recovery did not run"
}
if ($bootLog -notmatch '(?m)^soc_reconcile_rc=0$' -or
    $bootLog -notmatch 'soc_reconcile=.*(?:soc_reconcile_applied=1|reason=already_aligned)') {
    throw "Boot-time SOC reconciliation did not run"
}
if ($bootLog -notmatch '(?m)^vintf_debugfs_override=mounted$' -or
    $bootLog -notmatch '(?m)^vintf_debugfs_override_rc=0$') {
    throw "Boot-time DEBUG_FS VINTF compatibility override did not mount"
}
$vintfHash = (
    & $Adb -s $Device shell 'su -c "sha256sum /system/etc/vintf/compatibility_matrix.5.xml"' 2>&1
) -join "`n"
if ($vintfHash -notmatch '^961a5ff2e4b833eb7c7ac229ab8547fae49b61c0856d0c56446b03f6a322576a\s') {
    throw "DEBUG_FS VINTF compatibility override is not active"
}

$batteryState = (
    & $Adb -s $Device shell 'cat /sys/class/power_supply/battery/capacity; cat /sys/class/power_supply/battery/charge_counter; cat /sys/class/power_supply/battery/charge_now; cat /sys/class/power_supply/battery/charge_full' 2>&1
) | Where-Object { $_ -match '^\d+$' }
if ($batteryState.Count -eq 4) {
    $level = [int]$batteryState[0]
    $chargeCounter = [int]$batteryState[1]
    $chargeNow = [int]$batteryState[2]
    $chargeFull = [int]$batteryState[3]
    if ($chargeFull -gt 0) {
        $normalizedNow = if ($chargeCounter -gt 0) {
            $chargeCounter
        } elseif ($chargeFull -gt 100000) {
            $chargeNow * 1000
        } else {
            $chargeNow
        }
        $chargeLevel = [Math]::Round(($normalizedNow * 100.0) / $chargeFull)
        if ([Math]::Abs($level - $chargeLevel) -gt 10) {
            throw "Battery level disagrees with reconciled charge"
        }
    }
}

$crashAudit = (
    & $Adb -s $Device shell 'su -c "cat /data/adb/pftm10-control/crash-audit.log"' 2>&1
) -join "`n"
$bootId = (
    & $Adb -s $Device shell 'su -c "cat /proc/sys/kernel/random/boot_id"' 2>&1
).Trim()
if (-not $bootId) {
    throw "Kernel boot ID is unavailable"
}
$currentCrashAudit = $crashAudit -split '(?m)(?=^event=boot_audit$)' |
    Where-Object {
        $_ -match ('(?m)^boot_id=' + [regex]::Escape($bootId) + '$') -and
        $_ -match '(?m)^audit_stage=boot_completed$' -and
        $_ -match '(?m)^pstore_verdict=(?:panic|warning|other)$'
    } |
    Select-Object -First 1
if (-not $currentCrashAudit) {
    throw "Current-boot crash audit is incomplete"
}

$charge3aArmed = (
    & $Adb -s $Device shell 'su -c "test -f /data/adb/pftm10-control/charge-3a.enabled; echo $?"' 2>&1
) -join "`n"
$pdContract = (
    & $Adb -s $Device shell "su -c '/data/adb/modules/pftm10-control/bin/pftm10ctl kprobe-ctl pd-contract'" 2>&1
) -join "`n"
$charger3aMode = 'disabled'
if ($charge3aArmed -match '(?m)^0$') {
    if ($pdContract -match 'pd_connected=1' -and
        $pdContract -match 'contract_ma=3000') {
        if ($bootLog -notmatch '(?m)^charger_3a_rc=0$' -or
            $bootLog -notmatch 'charger_3a_applied=1') {
            throw "Armed 5V/3A charger policy did not apply at boot"
        }
        $charger3aMode = 'ready'
    } else {
        if ($bootLog -notmatch '(?m)^charger_3a=enabled$' -or
            $bootLog -notmatch '(?m)^charger_3a_watchdog_pid=\d+$') {
            throw "Armed charger policy watchdog is not running"
        }
        $charger3aMode = 'waiting_pd'
    }
}

Write-Output (
    "DEVICE_CHECK_OK module={0} versionCode={1} health=ok capabilities=ok kpm=1.4.6 control=ok cpu_max={3} crash_audit=ok charger_unsuspend=ok charger_3a={2} soc_reconcile=ok vintf_debugfs_override=mounted battery_restrict=masked privacy=camera_location_denied,oshare_bg_denied event=post-fs-data" -f
        $module.version,
        $module.versionCode,
        $charger3aMode,
        $cpuMaxMode
)
