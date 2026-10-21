# Deploy research build, launch Darkness II, capture render pipeline trace, watchdog hang detection.
param(
    [int]$MenuSeconds = 45,
    [int]$GameplaySeconds = 90,
    [switch]$SkipGameplay,
    [switch]$SkipDeploy,
    [int]$MaxPresentStallSec = 45,
    [int]$TransitionCount = 0
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$traceDir = Join-Path $env:LOCALAPPDATA 'Darkness2VR'
$archive = Join-Path $traceDir 'trace-archive'
New-Item -ItemType Directory -Force -Path $archive | Out-Null

$game = & (Join-Path $PSScriptRoot 'lib\Find-Darkness2Game.ps1')
Write-Host "Game: $($game.GameDir)"
Write-Host "Exe:  $($game.ExePath)"

if (-not $SkipDeploy) {
    & (Join-Path $PSScriptRoot 'deploy-research-build.ps1') -Install -DllOnly | Format-List *
    & (Join-Path $PSScriptRoot 'lib\Set-GameRenderIni.ps1') -GameDir $game.GameDir -Keys @{
        RenderPipelineTrace = 1
        RenderPipelineTraceMax = 2000000
        RenderPipelineTraceScenes = 0
        FrameBoundaryPolicy = 2
        SubmitMode = 0
        StereoMode = 0
        StartupMode = 1
        DisableOpenXR = 1
        HookMask = 2
    }
}

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
foreach ($name in @('render_pipeline_trace.csv', 'render_pipeline_retaddr.csv', 'd3d9.log')) {
    $src = Join-Path $traceDir $name
    if (Test-Path $src) {
        Move-Item $src (Join-Path $archive "$stamp-$name") -Force
    }
}

if (Get-Process DarknessII -ErrorAction SilentlyContinue) {
    Write-Host 'Stopping existing DarknessII process...'
    Stop-Process -Name DarknessII -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 3
}

Write-Host 'Launching steam://run/67370'
Start-Process 'steam://run/67370'
$deadline = (Get-Date).AddMinutes(3)
while (-not (Get-Process DarknessII -ErrorAction SilentlyContinue)) {
    if ((Get-Date) -gt $deadline) { throw 'Game did not start within 3 minutes' }
    Start-Sleep -Seconds 2
}
Write-Host "Game pid=$((Get-Process DarknessII).Id)"

function Get-TraceTailStats {
    param([string]$Path)
    if (-not (Test-Path $Path)) { return $null }
    $lines = Get-Content $Path -Tail 400 -ErrorAction SilentlyContinue
    if (-not $lines) { return $null }
    $events = @()
    foreach ($line in $lines) {
        if ($line -match '^ms,') { continue }
        $p = $line -split ','
        if ($p.Count -ge 3) { $events += $p[2] }
    }
    $present = ($events | Where-Object { $_ -in @('DevicePresent','PresentEx','SwapChainPresent') }).Count
    $end = ($events | Where-Object { $_ -eq 'EndScene' }).Count
    return @{ PresentTail = $present; EndSceneTail = $end; Lines = $lines.Count }
}

$tracePath = Join-Path $traceDir 'render_pipeline_trace.csv'
$logPath = Join-Path $traceDir 'd3d9.log'
$watchStart = Get-Date
$lastTraceGrowth = Get-Date
$lastPresentCount = 0

function Watchdog-Tick {
    $live = Get-Process DarknessII -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $live) {
        Write-Warning 'Game process exited (capture partial trace if present)'
        return $false
    }
    if (Test-Path $tracePath) {
        $fi = Get-Item $tracePath
        if ($fi.Length -gt 0) { $script:lastTraceGrowth = Get-Date }
    }
    if (((Get-Date) - $lastTraceGrowth).TotalSeconds -gt $MaxPresentStallSec) {
        Write-Warning "WATCHDOG: no trace growth for ${MaxPresentStallSec}s"
        $script:lastTraceGrowth = Get-Date
    }
    if (Test-Path $logPath) {
        $tail = Get-Content $logPath -Tail 5 -ErrorAction SilentlyContinue
        if ($tail -match 'VrFrame called') {
            Write-Warning 'WATCHDOG: VrFrame activity detected during trace-only run (unexpected)'
        }
    }
}

Write-Host "Menu capture ${MenuSeconds}s..."
$menuEnd = (Get-Date).AddSeconds($MenuSeconds)
while ((Get-Date) -lt $menuEnd) {
    if (-not (Watchdog-Tick)) { break }
    Start-Sleep -Seconds 2
}

if (-not $SkipGameplay) {
    Write-Host 'Transition to gameplay (load_gameplay.ps1)...'
    & (Join-Path $repo 'test\load_gameplay.ps1') -WaitLoad $GameplaySeconds
    $gpEnd = (Get-Date).AddSeconds(5)
    while ((Get-Date) -lt $gpEnd) { Watchdog-Tick; Start-Sleep 1 }
}

if ($TransitionCount -gt 0) {
    for ($i = 1; $i -le $TransitionCount; $i++) {
        Write-Host "Transition loop $i / $TransitionCount"
        if (Get-Process DarknessII -ErrorAction SilentlyContinue) {
            & (Join-Path $repo 'test\load_gameplay.ps1') -WaitLoad 60
        }
    }
}

Write-Host 'Stopping game...'
Get-Process DarknessII -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 2

$fail = $false
if (-not (Test-Path $logPath)) {
    Write-Error 'FAIL: d3d9.log not created'
    $fail = $true
} else {
    $cfg = Select-String -Path $logPath -Pattern '\[EffectiveConfig\]' | Select-Object -Last 3
    if (-not $cfg) { Write-Error 'FAIL: EffectiveConfig not logged'; $fail = $true }
    else { $cfg | ForEach-Object { Write-Host $_.Line } }
}
if (-not (Test-Path $tracePath)) {
    Write-Error 'FAIL: render_pipeline_trace.csv not created'
    $fail = $true
} else {
    $lines = (Get-Content $tracePath | Measure-Object -Line).Lines
    if ($lines -lt 50) {
        Write-Error "FAIL: trace too short ($lines lines)"
        $fail = $true
    } else {
        Copy-Item $tracePath (Join-Path $archive "$stamp-final-trace.csv") -Force
        python (Join-Path $PSScriptRoot 'analyze-render-pipeline-trace.py') $tracePath
        if ($LASTEXITCODE -ne 0) { $fail = $true }
        $scPresent = (Select-String -Path $tracePath -Pattern ',SwapChainPresent,' -AllMatches).Count
        if ($scPresent -lt 30) {
            Write-Error "FAIL: SwapChainPresent count=$scPresent (expected >=30 for menu capture)"
            $fail = $true
        }
    }
}
if ($fail) { exit 1 }
Write-Host "Archive: $archive\$stamp-*"
Write-Host 'PASS trace capture'
