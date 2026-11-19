[CmdletBinding()]
param(
    [string]$Adb = 'C:\Users\SYT\AppData\Local\Android\Sdk\platform-tools\adb.exe',
    [string]$AgentVerifier = 'E:\AI\pftm10\agent-20261114\verify-agent.ps1'
)

$ErrorActionPreference = 'Stop'
$failures = New-Object System.Collections.Generic.List[string]

function Add-Check {
    param([string]$Name, [bool]$Pass, [string]$Value)

    if ($Pass) {
        Write-Output "PASS $Name $Value"
    } else {
        Write-Output "FAIL $Name $Value"
        $script:failures.Add($Name)
    }
}

function Invoke-RootShell {
    param([string]$Command)

    $output = & $Adb shell "su -c '$Command'" 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "root shell failed: $Command"
    }
    return ($output -join "`n").Trim()
}

$props = Invoke-RootShell 'getprop sys.boot_completed; getprop ro.build.display.id; getprop ro.boot.flash.locked; getprop ro.boot.verifiedbootstate; getprop init.svc.tee-1-1; getprop vendor.mtk.md1.status'
Add-Check 'boot_completed' ($props -match '(?m)^1$') 'boot_completed=1'
Add-Check 'locked' ($props -match '(?m)^0$') 'flash.locked=0'
Add-Check 'verified_orange' ($props -match '(?m)^orange$') 'verifiedbootstate=orange'
Add-Check 'tee' ($props -match '(?m)^running$') 'tee-1-1=running'
Add-Check 'modem' ($props -match '(?m)^ready$') 'md1=ready'

$modules = Invoke-RootShell '/data/adb/apd module list'
foreach ($item in @(
    @('pftm10-control', '1.5.7'),
    @('pftm10-agent', '0.4.5'),
    @('pftm10-kernel-modules', '1.0.7')
)) {
    $id = $item[0]
    $version = $item[1]
    $pass = (
        $modules -match [regex]::Escape('"id": "' + $id + '"') -and
        $modules -match [regex]::Escape('"version": "' + $version + '"')
    )
    Add-Check "module_$id" $pass "version=$version"
}

$hashFiles = @(
    @{
        Local = 'E:\AI\pftm10\pftm10-apatch-toolkit\modules\pftm10-control\module.prop'
        Device = '/data/adb/modules/pftm10-control/module.prop'
    },
    @{
        Local = 'E:\AI\pftm10\pftm10-apatch-toolkit\modules\pftm10-control\bin\pftm10ctl'
        Device = '/data/adb/modules/pftm10-control/bin/pftm10ctl'
    },
    @{
        Local = 'E:\AI\pftm10\control-20261116-kpmctl-lock\dist\stage\bin\pftm10-kpmctl'
        Device = '/data/adb/modules/pftm10-control/bin/pftm10-kpmctl'
    },
    @{
        Local = 'E:\AI\pftm10\pftm10-apatch-toolkit\modules\pftm10-control\bin\charge-3a-watchdog.sh'
        Device = '/data/adb/modules/pftm10-control/bin/charge-3a-watchdog.sh'
    },
    @{
        Local = 'E:\AI\pftm10\pftm10-apatch-toolkit\modules\pftm10-control\bin\cpu-max-watchdog.sh'
        Device = '/data/adb/modules/pftm10-control/bin/cpu-max-watchdog.sh'
    },
    @{
        Local = 'E:\AI\pftm10\pftm10-apatch-toolkit\artifacts\stage\bin\pftm10-cpuholdctl'
        Device = '/data/adb/modules/pftm10-control/bin/pftm10-cpuholdctl'
    },
    @{
        Local = 'E:\AI\pftm10\pftm10-apatch-toolkit\kpms\pftm10-cpuhold\pftm10-cpuhold.kpm'
        Device = '/data/adb/modules/pftm10-control/kpm/pftm10-cpuhold.kpm'
    },
    @{
        Local = 'E:\AI\pftm10\pftm10-apatch-toolkit\artifacts\pftm10-kprobe.kpm'
        Device = '/data/adb/ap/kpm/pftm10-kprobe/pftm10-kprobe.kpm'
    },
    @{
        Local = 'E:\AI\pftm10\pftm10-apatch-toolkit\modules\pftm10-control\bin\boot-audit.sh'
        Device = '/data/adb/modules/pftm10-control/bin/boot-audit.sh'
    },
    @{
        Local = 'E:\AI\pftm10\pftm10-apatch-toolkit\modules\pftm10-control\post-fs-data.sh'
        Device = '/data/adb/modules/pftm10-control/post-fs-data.sh'
    },
    @{
        Local = 'E:\AI\pftm10\pftm10-apatch-toolkit\modules\pftm10-control\vintf\compatibility_matrix.5.xml'
        Device = '/data/adb/modules/pftm10-control/vintf/compatibility_matrix.5.xml'
    },
    @{
        Local = 'E:\AI\pftm10\custom-kernel-20261114\module-loader-20261115\src\module.prop'
        Device = '/data/adb/modules/pftm10-kernel-modules/module.prop'
    },
    @{
        Local = 'E:\AI\pftm10\custom-kernel-20261114\module-loader-20261115\src\bin\load-modules.sh'
        Device = '/data/adb/modules/pftm10-kernel-modules/bin/load-modules.sh'
    },
    @{
        Local = 'E:\AI\pftm10\agent-20261114\pftm10-agent'
        Device = '/data/adb/modules/pftm10-agent/bin/pftm10-agent'
    },
    @{
        Local = 'E:\AI\pftm10\agent-20261114\module\pftm10-agent\bin\supervise-agent.sh'
        Device = '/data/adb/modules/pftm10-agent/bin/supervise-agent.sh'
    }
)

foreach ($entry in $hashFiles) {
    $localHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $entry.Local).Hash
    $remoteHash = (Invoke-RootShell "sha256sum $($entry.Device)").Split()[0].ToUpperInvariant()
    Add-Check "hash_$(Split-Path -Leaf $entry.Device)" ($localHash -eq $remoteHash) $remoteHash
}

$agent = Invoke-RootShell '/data/adb/modules/pftm10-agent/bin/pftm10-agent call health'
Add-Check 'agent_health' ($agent -match 'health=ok') 'health=ok'
Add-Check 'agent_kpm' ($agent -match 'kprobe_version=1\.4\.6') 'kprobe=1.4.6'

$supervisor = Invoke-RootShell '/system/bin/sh /data/adb/modules/pftm10-agent/bin/supervise-agent.sh status'
Add-Check 'supervisor' ($supervisor -match 'supervisor_pid=\d+ alive=1') 'alive=1'
Add-Check 'agent_server' ($supervisor -match 'server_pid=\d+ alive=1') 'alive=1'
Add-Check 'agent_socket' ($supervisor -match 'socket=present') 'present'

$secondRun = Invoke-RootShell '/system/bin/sh /data/adb/modules/pftm10-agent/bin/supervise-agent.sh run'
Add-Check 'agent_singleton' ($secondRun -match '^supervisor_already_running pid=\d+$') $secondRun

$control = Invoke-RootShell '/data/adb/modules/pftm10-control/bin/pftm10ctl health'
Add-Check 'control_health' ($control -match 'health=ok') 'health=ok'
Add-Check 'control_kprobe' ($control -match 'kprobe_control=ok') 'kprobe_control=ok'

$cpuMaxDeadline = (Get-Date).AddSeconds(60)
do {
    $cpuMax = Invoke-RootShell '/data/adb/modules/pftm10-control/bin/pftm10ctl cpu-max status'
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

$cpuMaxMode = if ($cpuMax -match '(?m)^cpu_max=(disabled|armed)$') {
    $matches[1]
} else {
    'invalid'
}
Add-Check 'cpu_max_state' ($cpuMaxMode -in @('disabled', 'armed')) "cpu_max=$cpuMaxMode"
if ($cpuMaxMode -eq 'disabled') {
    Add-Check 'cpu_max_default_kpm' ($cpuMax -match '(?m)^cpu_max_kpm=unloaded$') 'unloaded'
} else {
    Add-Check 'cpu_max_kpm' ($cpuMax -match '(?m)^cpu_max_kpm=loaded$') 'loaded'
    Add-Check 'cpu_max_watchdog' ($cpuMax -match '(?m)^cpu_max_watchdog=running$') 'running'
}
Add-Check 'control_cpuholdctl' ($cpuMax -match '(?m)^cpu_max_target_khz=2400000$') 'target=2400000'
if ($cpuMaxMode -eq 'armed') {
    $cpuHoldControl = Invoke-RootShell '/data/adb/modules/pftm10-control/bin/pftm10-cpuholdctl help'
    Add-Check 'cpuhold_control' ($cpuHoldControl -match 'cpu-max:<khz>') 'cpu-max:<khz>'
} else {
    Add-Check 'cpuhold_control' $true 'deferred_until_armed'
}

$kernelState = Invoke-RootShell 'getprop pftm10.kernel_modules.state'
Add-Check 'kernel_modules_state' ($kernelState -eq 'loaded') $kernelState

$loaded = Invoke-RootShell 'lsmod'
foreach ($name in @(
    'trace_mmstat',
    'connfem',
    'wmt_drv',
    'wmt_chrdev_wifi',
    'bt_drv_connac1x',
    'wlan_drv_gen4m',
    'gps_drv',
    'fmradio_drv_mt6631_6635',
    'fpsgo',
    'met'
)) {
    Add-Check "module_$name" ($loaded -match "(?m)^$name\s") 'loaded'
}

& $AgentVerifier -Adb $Adb | ForEach-Object { Write-Output "AGENT $_" }
if ($LASTEXITCODE -ne 0) {
    $failures.Add('agent_verifier')
}

if ($failures.Count -ne 0) {
    Write-Output "PFTM10_RUNTIME_FAILED count=$($failures.Count)"
    exit 1
}

Write-Output "PFTM10_RUNTIME_OK control=1.5.7 agent=0.4.5 kernel_modules=1.0.7 cpu_max=$cpuMaxMode tee=running md1=ready"
exit 0
