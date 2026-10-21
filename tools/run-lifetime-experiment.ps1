# Controlled game lifetime experiments (A/B/C/D). Writes JSON summary under trace-archive/.
param(
    [ValidateSet('A','B','C','D','ALL')]
    [string]$Mode = 'ALL',
    [int]$DurationSec = 120
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$game = & (Join-Path $PSScriptRoot 'lib\Find-Darkness2Game.ps1')
$gameDir = $game.GameDir
$dll = Join-Path $gameDir 'd3d9.dll'
$dllOff = Join-Path $gameDir 'd3d9.dll.darkness2vr-off'
$logPath = Join-Path $env:LOCALAPPDATA 'Darkness2VR\d3d9.log'
$tracePath = Join-Path $env:LOCALAPPDATA 'Darkness2VR\render_pipeline_trace.csv'
$outDir = Join-Path $env:LOCALAPPDATA 'Darkness2VR\lifetime-experiments'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

function Stop-Game {
    Get-Process DarknessII -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 2
}

function Set-ModState([bool]$enabled) {
    if ($enabled) {
        if (Test-Path $dllOff) { Move-Item $dllOff $dll -Force }
    } else {
        if (Test-Path $dll) { Move-Item $dll $dllOff -Force }
    }
}

function Write-GameIni([hashtable]$kv) {
    & (Join-Path $PSScriptRoot 'lib\Set-GameRenderIni.ps1') -GameDir $gameDir -Keys $kv
}

function Wait-GameProcess {
    $deadline = (Get-Date).AddMinutes(4)
    while ((Get-Date) -lt $deadline) {
        $p = Get-Process DarknessII -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($p) { return $p }
        Start-Sleep -Seconds 2
    }
    return $null
}

function Monitor-Game([int]$seconds, [string]$label) {
    $start = Get-Date
    $first = Wait-GameProcess
    if (-not $first) {
        return [pscustomobject]@{
            Label = $label; Started = $false; DurationSec = 0; ExitReason = 'never_started'
            LastLogLine = $null; TraceBytes = 0; FinalPid = $null
        }
    }
    $reason = 'duration_complete'
    $lastPid = $first.Id
    while (((Get-Date) - $start).TotalSeconds -lt $seconds) {
        Start-Sleep -Seconds 2
        $p = Get-Process DarknessII -ErrorAction SilentlyContinue | Select-Object -First 1
        if (-not $p) {
            $reason = 'process_exited'
            break
        }
        $lastPid = $p.Id
    }
    $dur = [int]((Get-Date) - $start).TotalSeconds
    $lastLog = $null
    if (Test-Path $logPath) { $lastLog = (Get-Content $logPath -Tail 1 -ErrorAction SilentlyContinue) }
    $tb = if (Test-Path $tracePath) { (Get-Item $tracePath).Length } else { 0 }
    return [pscustomobject]@{
        Label = $label; Started = $true; DurationSec = $dur; ExitReason = $reason
        LastLogLine = $lastLog; TraceBytes = $tb; FinalPid = $lastPid
    }
}

function Run-One($id, $setupScriptBlock) {
    Write-Host "=== Experiment $id ===" -ForegroundColor Cyan
    Stop-Game
    if ($id -ne 'A') {
        & (Join-Path $PSScriptRoot 'deploy-research-build.ps1') -Install -DllOnly | Out-Null
    }
    & $setupScriptBlock
    Remove-Item $logPath -Force -ErrorAction SilentlyContinue
    Remove-Item $tracePath -Force -ErrorAction SilentlyContinue
    Start-Process 'steam://run/67370'
    $r = Monitor-Game -seconds $DurationSec -label $id
    Stop-Game
    $path = Join-Path $outDir ("{0}-{1:yyyyMMdd-HHmmss}.json" -f $id, (Get-Date))
    $r | ConvertTo-Json | Set-Content $path
    Write-Host ($r | Format-List | Out-String)
    return $r
}

$results = @()
$run = {
    param($m)
    if ($Mode -ne 'ALL' -and $Mode -ne $m) { return }
}

if ($Mode -eq 'ALL' -or $Mode -eq 'A') {
    $results += Run-One 'A' {
        Set-ModState $false
    }
}
if ($Mode -eq 'ALL' -or $Mode -eq 'B') {
    $results += Run-One 'B' {
        Set-ModState $true
        Write-GameIni @{
            RenderPipelineTrace = 0; FrameBoundaryPolicy = 2; SubmitMode = 0; StereoMode = 0
            StartupMode = 1; DisableOpenXR = 1; HookMask = 0
        }
    }
}
if ($Mode -eq 'ALL' -or $Mode -eq 'C') {
    $results += Run-One 'C' {
        Set-ModState $true
        Write-GameIni @{
            RenderPipelineTrace = 1; FrameBoundaryPolicy = 2; SubmitMode = 0; StereoMode = 0
            StartupMode = 1; DisableOpenXR = 1; HookMask = 2
        }
    }
}
if ($Mode -eq 'ALL' -or $Mode -eq 'D') {
    $results += Run-One 'D' {
        Set-ModState $true
        Write-GameIni @{
            RenderPipelineTrace = 1; FrameBoundaryPolicy = 2; SubmitMode = 0; StereoMode = 0
            StartupMode = 1; DisableOpenXR = 1; HookMask = 2
        }
    }
}

$results | ConvertTo-Json -Depth 4 | Write-Host
