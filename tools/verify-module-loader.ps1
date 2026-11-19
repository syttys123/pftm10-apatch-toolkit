[CmdletBinding()]
param(
    [string]$Root = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath($Root)

$modulePropPath = Join-Path $repoRoot 'module-loader\src\module.prop'
$loaderPath = Join-Path $repoRoot 'module-loader\src\bin\load-modules.sh'
$customizePath = Join-Path $repoRoot 'module-loader\src\customize.sh'

$moduleProp = Get-Content -Raw -LiteralPath $modulePropPath
if ($moduleProp -notmatch '(?m)^version=1\.0\.7$' -or
    $moduleProp -notmatch '(?m)^versionCode=107$') {
    throw 'module-loader version is not 1.0.7/107'
}

$expectedKernels = @(
    'Sat Nov 14 21:43:18 CST 2026',
    'Sun Nov 15 17:28:38 CST 2026',
    'Mon Nov 16 08:35:50 CST 2026',
    'Tue Nov 17 13:24:56 CST 2026'
)

$loader = Get-Content -Raw -LiteralPath $loaderPath
foreach ($kernel in $expectedKernels) {
    if (-not $loader.Contains($kernel)) {
        throw "module-loader is missing kernel timestamp: $kernel"
    }
}
if ($loader -notmatch 'expected_kernels=nov14,nov15,nov16,nov17') {
    throw 'module-loader expected-kernel log line is stale'
}

$documentation = @(
    'module-loader\README.md',
    'module-loader\src\README.md',
    'docs\06-modules.md'
)
foreach ($relativePath in $documentation) {
    $content = Get-Content -Raw -LiteralPath (Join-Path $repoRoot $relativePath)
    foreach ($kernel in $expectedKernels) {
        if (-not $content.Contains($kernel)) {
            throw "$relativePath is missing kernel timestamp: $kernel"
        }
    }
}

$customize = Get-Content -Raw -LiteralPath $customizePath
foreach ($entryPoint in @(
    '$MODPATH/post-fs-data.sh',
    '$MODPATH/service.sh',
    '$MODPATH/bin/load-modules.sh'
)) {
    if (-not $customize.Contains("set_perm `"$entryPoint`" 0 0 0755")) {
        throw "customize.sh does not set 0755 on $entryPoint"
    }
}

Write-Output 'MODULE_LOADER_OK version=1.0.7 kernel_timestamps=4 docs=3 entry_points=3'
