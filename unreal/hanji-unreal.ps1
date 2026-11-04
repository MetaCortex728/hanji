# SPDX-License-Identifier: Apache-2.0
<#
.SYNOPSIS
Builds the Hanji Unreal plugin through the sample project, regenerates its assets, and runs its automation tests.

.DESCRIPTION
GitHub's runners have no Unreal Engine, so this script is how the plugin is built and tested.

  -Mode Build    compile HanjiSampleEditor (the plugin and the core with it)
  -Mode Assets   run Scripts/make_assets.py: the quad mesh and the sprite materials (committed assets)
  -Mode Test     run the "Hanji." automation tests headless and check the report
  -Mode All      Build, then Test (default)

The engine defaults to C:\Program Files\Epic Games\UE_5.8; pass -EngineRoot or set UE_ROOT otherwise.
Shader compilation goes through the local machine only (-noxgeshadercompile), because a distributed
compiler that is installed but not licensed stalls it.
#>
param(
    [ValidateSet('Build', 'Assets', 'Test', 'All')]
    [string]$Mode = 'All',
    [string]$EngineRoot = $(if ($env:UE_ROOT) { $env:UE_ROOT } else { 'C:\Program Files\Epic Games\UE_5.8' })
)

$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$project = Join-Path $here 'HanjiSample\HanjiSample.uproject'
$saved = Join-Path $here 'HanjiSample\Saved'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$build = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
New-Item -ItemType Directory -Force $saved | Out-Null

function Invoke-Build {
    & $build HanjiSampleEditor Win64 Development "-Project=$project" -WaitMutex -NoHotReloadFromIDE -NoUBA -NoXGE `
        "-Log=$(Join-Path $saved 'HanjiBuild.log')"
    if ($LASTEXITCODE -ne 0) { throw "Build failed ($LASTEXITCODE); see $(Join-Path $saved 'HanjiBuild.log')" }
}

function Invoke-Assets {
    $script = Join-Path $here 'Hanji\Scripts\make_assets.py'
    $log = Join-Path $saved 'HanjiAssets.log'
    & $editor $project -run=pythonscript "-script=$script" -unattended -nop4 -nosplash -NoSound -noxgeshadercompile "-abslog=$log"
    if ($LASTEXITCODE -ne 0) { throw "Asset script failed ($LASTEXITCODE); see $log" }
    if (-not (Select-String -Path $log -Pattern '\[hanji\] done' -Quiet)) { throw "Asset script did not finish; see $log" }
}

function Invoke-Test {
    $reports = Join-Path $saved 'Automation\Hanji'
    $log = Join-Path $saved 'HanjiAutomation.log'
    $startedAt = Get-Date
    & $editor $project /Engine/Maps/Entry -unattended -nop4 -NullRHI -NoSound -NoSplash -noxgeshadercompile `
        '-ExecCmds=Automation RunTests Hanji.' '-TestExit=Automation Test Queue Empty' `
        "-ReportExportPath=$reports" "-abslog=$log"
    $exit = $LASTEXITCODE
    $index = Join-Path $reports 'index.json'
    if (-not (Test-Path $index) -or (Get-Item $index).LastWriteTime -lt $startedAt) {
        throw "No fresh automation report (exit $exit); process exit alone is not test success. See $log"
    }
    $report = Get-Content $index -Raw -Encoding UTF8 | ConvertFrom-Json
    foreach ($t in $report.tests) {
        $mark = if ($t.state -eq 'Success') { 'PASS' } else { 'FAIL' }
        "{0}  {1}" -f $mark, $t.fullTestPath
        if ($t.state -ne 'Success') {
            foreach ($e in $t.entries) { if ($e.event.type -ne 'Info') { "      " + $e.event.message } }
        }
    }
    "{0} succeeded, {1} failed, {2} not run" -f $report.succeeded, $report.failed, $report.notRun
    if ($report.failed -gt 0 -or $report.succeeded -eq 0) { throw 'Automation tests failed.' }
}

switch ($Mode) {
    'Build' { Invoke-Build }
    'Assets' { Invoke-Assets }
    'Test' { Invoke-Test }
    'All' { Invoke-Build; Invoke-Test }
}
