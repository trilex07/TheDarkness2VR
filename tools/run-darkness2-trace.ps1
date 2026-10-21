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
    & (Join-Path $PSScriptRoot 'deploy-research-build.ps1') -Install | Format-List *
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
$proc = Get-Process DarknessII
Write-Host "Game pid=$($proc.Id)"

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
    if (-not (Get-Process -Id $proc.Id -ErrorAction SilentlyContinue)) {
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
Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 2

if (Test-Path $tracePath) {
    Copy-Item $tracePath (Join-Path $archive "$stamp-final-trace.csv") -Force
    python (Join-Path $PSScriptRoot 'analyze-render-pipeline-trace.py') $tracePath
} else {
    Write-Warning "No trace captured at $tracePath"
}

Write-Host "Archive: $archive\$stamp-*"
Write-Host 'DONE'
