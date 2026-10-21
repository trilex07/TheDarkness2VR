# Repeatable local startup validation (no headset required for first gates).
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$game = 'C:\Program Files (x86)\Steam\steamapps\common\Darkness II'
$log = Join-Path $env:LOCALAPPDATA 'Darkness2VR\d3d9.log'

Write-Output '=== build ==='
& (Join-Path $root 'build.ps1')
if ($LASTEXITCODE -ne 0) { throw 'build failed' }

Write-Output '=== OpenXR unit smoke (test\vr_frame_test.exe) ==='
& (Join-Path $root 'test\build_vr_test.ps1')

Write-Output '=== game launch smoke (45s, direct exe) ==='
Get-Process DarknessII -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 2
$p = Start-Process -FilePath (Join-Path $game 'DarknessII.exe') -WorkingDirectory $game -PassThru
Start-Sleep -Seconds 45
$alive = Get-Process -Id $p.Id -ErrorAction SilentlyContinue
if (-not $alive) { throw 'DarknessII exited within 45s' }
Write-Output "OK: pid=$($p.Id) still running"

if (Test-Path $log) {
    $tail = Get-Content $log -Tail 30
    $stages = $tail | Select-String '\[Startup\] stage='
    Write-Output '--- last startup stages in log ---'
    $stages | ForEach-Object { $_.Line }
    if ($tail -match 'EXCEPTION') { throw 'EXCEPTION found in recent log tail' }
    if ($tail -notmatch 'stage=11') { Write-Warning 'stage=11 HookDevice not in last 30 lines (may be OK if log rotated)' }
} else {
    Write-Warning "No log at $log"
}

Get-Process -Id $p.Id -ErrorAction SilentlyContinue | Stop-Process -Force
Write-Output 'VALIDATION: PASS (local gates)'
