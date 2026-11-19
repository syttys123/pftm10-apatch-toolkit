[CmdletBinding()]
param(
    [string]$Adb = 'C:\Users\SYT\AppData\Local\Android\Sdk\platform-tools\adb.exe',
    [string]$Device,
    [ValidateRange(10, 300)]
    [int]$Seconds = 35
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $Adb)) {
    throw "ADB not found"
}

if (-not $Device) {
    $available = @(
        & $Adb devices |
            Select-Object -Skip 1 |
            ForEach-Object {
                if ($_ -match '^([^\s]+)\s+device$') {
                    $matches[1]
                }
            }
    )
    if ($available.Count -ne 1) {
        throw "Expected exactly one online ADB device"
    }
    $Device = $available[0]
}

function Invoke-RootShell {
    param([string]$Command)

    $output = & $Adb -s $Device shell "su -c '$Command'" 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "root shell failed: $Command"
    }
    return ($output -join "`n").Trim()
}

function Get-ChargerState {
    $sentinel = Invoke-RootShell 'if [ -f /data/adb/pftm10-control/charge-3a.enabled ]; then echo present; else echo absent; fi'
    $watchdog = Invoke-RootShell 'pid=$(cat /data/adb/pftm10-control/charge-3a.pid 2>/dev/null); if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then echo running; else echo missing; fi'
    $lastLine = Invoke-RootShell 'tail -n 1 /data/adb/pftm10-control/charge-3a.log 2>/dev/null'
    $lastState = if ($lastLine -match 'state=([^ ]+)') {
        $matches[1]
    } else {
        'unknown'
    }

    return [ordered]@{
        sentinel = $sentinel
        watchdog = $watchdog
        last_state = $lastState
        usb_online = Invoke-RootShell 'cat /sys/class/power_supply/usb/online 2>/dev/null'
        usb_type = Invoke-RootShell 'cat /sys/class/power_supply/usb/type 2>/dev/null'
        usb_current_max = Invoke-RootShell 'cat /sys/class/power_supply/usb/current_max 2>/dev/null'
        usb_voltage_max = Invoke-RootShell 'cat /sys/class/power_supply/usb/voltage_max 2>/dev/null'
        charger_3a_calls = Invoke-RootShell 'dmesg | grep -c "control0 command: charger-3a" || true'
    }
}

$before = Get-ChargerState

if ($before.sentinel -ne 'present') {
    Write-Output "CHARGER_IDLE_SKIP reason=disabled"
    exit 0
}
if ($before.watchdog -ne 'running') {
    throw "charge-3a sentinel is armed but the watchdog is not running"
}

$usbOnline = $before.usb_online
$usbType = $before.usb_type
$currentMax = 0
$voltageMax = 0
[void][int]::TryParse($before.usb_current_max, [ref]$currentMax)
[void][int]::TryParse($before.usb_voltage_max, [ref]$voltageMax)
$pdCandidate = (
    $usbType -in @('USB_PD', 'USB_PD_DRP') -or
    $currentMax -ge 3000000 -or
    $voltageMax -ge 9000000
)

if ($pdCandidate) {
    Write-Output "CHARGER_IDLE_SKIP reason=pd_capable usb_type=$usbType usb_current_max=$currentMax usb_voltage_max=$voltageMax"
    exit 0
}

$callsBefore = [int]$before.charger_3a_calls
Start-Sleep -Seconds $Seconds

$after = Get-ChargerState

if ($after.watchdog -ne 'running') {
    throw "charge-3a watchdog stopped during the idle check"
}
if ($after.last_state -eq 'pd_pending') {
    throw "watchdog entered pd_pending without a PD-capable USB source"
}

$callsAfter = [int]$after.charger_3a_calls
if ($callsAfter -gt $callsBefore) {
    throw "charger-3a was called while no PD-capable source was present"
}

Write-Output (
    "CHARGER_IDLE_OK seconds={0} charger_3a_calls_delta={1} state={2} usb_online={3} usb_type={4} usb_current_max={5} usb_voltage_max={6}" -f
        $Seconds,
        ($callsAfter - $callsBefore),
        $after.last_state,
        $after.usb_online,
        $after.usb_type,
        $after.usb_current_max,
        $after.usb_voltage_max
)
