# Overlay x SwapChain hook matrix (A/B/C/D). Writes JSON under present-hook-experiments/.
param(
    [int]$DurationSec = 90,
    [switch]$SkipRestoreOverlay
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$out = Join-Path $env:LOCALAPPDATA 'Darkness2VR\present-hook-experiments'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$game = & (Join-Path $PSScriptRoot 'lib\Find-Darkness2Game.ps1')
$setIni = Join-Path $PSScriptRoot 'lib\Set-GameRenderIni.ps1'
$setOverlay = Join-Path $PSScriptRoot 'lib\Set-SteamOverlay.ps1'

function Stop-Game {
    Get-Process DarknessII -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep 2
}

function Test-OverlayLoaded {
    $p = Get-Process DarknessII -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $p) { return $false }
    return [bool]($p.Modules.ModuleName -contains 'GameOverlayRenderer.dll')
}

function Run-Case($id, $hookOn, $overlayOn) {
    Write-Host "=== Case $id hook=$hookOn overlay=$overlayOn ===" -ForegroundColor Cyan
    Stop-Game
    if ($overlayOn) { & $setOverlay -State On } else { & $setOverlay -State Off }
    & (Join-Path $PSScriptRoot 'deploy-research-build.ps1') -Install -DllOnly | Out-Null
    & $setIni -GameDir $game.GameDir -Keys @{
        DisableOpenXR = 1; SubmitMode = 0; StartupMode = 1; RenderPipelineTrace = 0
        PresentHookLevel = 0; HookMask = $(if ($hookOn) { 2 } else { 0 })
    }
    Remove-Item (Join-Path $env:LOCALAPPDATA 'Darkness2VR\d3d9.log') -Force -ErrorAction SilentlyContinue
    Start-Process 'steam://run/67370'
    $deadline = (Get-Date).AddMinutes(4)
    while (-not (Get-Process DarknessII -ErrorAction SilentlyContinue)) {
        if ((Get-Date) -gt $deadline) { break }
        Start-Sleep 2
    }
    $started = [bool](Get-Process DarknessII -ErrorAction SilentlyContinue)
    $overlayLoaded = if ($started) { Test-OverlayLoaded } else { $false }
    $t0 = Get-Date
    $reason = 'duration_complete'
    while ($started -and ((Get-Date) - $t0).TotalSeconds -lt $DurationSec) {
        if (-not (Get-Process DarknessII -ErrorAction SilentlyContinue)) { $reason = 'process_exited'; break }
        Start-Sleep 2
    }
    $dur = [int]((Get-Date) - $t0).TotalSeconds
    $presentCount = $null
    $log = Join-Path $env:LOCALAPPDATA 'Darkness2VR\d3d9.log'
    if (Test-Path $log) {
        $m = Select-String -Path $log -Pattern 'PresentHook\] detach presentCount=(\d+)' | Select-Object -Last 1
        if (-not $m) { $m = Select-String -Path $log -Pattern 'PresentHook\] SwapChainPresent count=(\d+)' | Select-Object -Last 1 }
        if ($m -and $m.Matches.Groups.Count -gt 1) { $presentCount = [int]$m.Matches.Groups[1].Value }
    }
    Stop-Game
    [pscustomobject]@{
        Case = $id; HookMask = $(if ($hookOn) { 2 } else { 0 }); OverlayRequested = $overlayOn
        OverlayModuleLoaded = $overlayLoaded; DurationSec = $dur; ExitReason = $reason
        PresentCountLog = $presentCount; Started = $started
    }
}

$results = @(
    (Run-Case 'A' $false $true)
    (Run-Case 'B' $false $false)
    (Run-Case 'C' $true $false)
    (Run-Case 'D' $true $true)
)
if (-not $SkipRestoreOverlay) { & $setOverlay -State Restore }
$path = Join-Path $out ("matrix-{0:yyyyMMdd-HHmmss}.json" -f (Get-Date))
$results | ConvertTo-Json -Depth 4 | Set-Content $path
$results | Format-Table
Write-Host "Saved $path"
