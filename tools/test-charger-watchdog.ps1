[CmdletBinding()]
param(
    [string]$Adb = 'C:\Users\SYT\AppData\Local\Android\Sdk\platform-tools\adb.exe',
    [string]$Device
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

$repoRoot = Split-Path -Parent $PSScriptRoot
$testRoot = '/data/local/tmp/pftm10-charger-watchdog-test'
$watchdog = Join-Path $repoRoot 'modules\pftm10-control\bin\charge-3a-watchdog.sh'
$fakeControl = Join-Path $PSScriptRoot 'testdata\charger-watchdog\fake-pftm10ctl.sh'
$runner = Join-Path $PSScriptRoot 'testdata\charger-watchdog\run-cases.sh'

& $Adb -s $Device shell "rm -rf $testRoot"
if ($LASTEXITCODE -ne 0) {
    throw "Cannot prepare device test directory"
}

try {
    & $Adb -s $Device shell "mkdir -p $testRoot"
    if ($LASTEXITCODE -ne 0) {
        throw "Cannot create device test directory"
    }

    & $Adb -s $Device push $watchdog "$testRoot/watchdog.sh"
    if ($LASTEXITCODE -ne 0) {
        throw "Watchdog push failed"
    }
    & $Adb -s $Device push $fakeControl "$testRoot/fake-pftm10ctl.sh"
    if ($LASTEXITCODE -ne 0) {
        throw "Fake control push failed"
    }
    & $Adb -s $Device push $runner "$testRoot/run-cases.sh"
    if ($LASTEXITCODE -ne 0) {
        throw "Test runner push failed"
    }

    & $Adb -s $Device shell "chmod 0755 $testRoot/watchdog.sh $testRoot/fake-pftm10ctl.sh $testRoot/run-cases.sh"
    if ($LASTEXITCODE -ne 0) {
        throw "Cannot set test permissions"
    }

    $output = & $Adb -s $Device shell "/system/bin/sh $testRoot/run-cases.sh $testRoot" 2>&1
    $output | ForEach-Object { Write-Output $_ }
    if ($LASTEXITCODE -ne 0) {
        throw "Charger watchdog branch tests failed"
    }
} finally {
    & $Adb -s $Device shell "rm -rf $testRoot" | Out-Null
}
