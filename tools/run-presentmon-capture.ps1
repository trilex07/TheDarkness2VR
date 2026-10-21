# Non-invasive PresentMon capture for Darkness II (HookMask=0 baseline).
param(
    [int]$DurationSec = 90,
    [string]$ProcessName = 'DarknessII',
    [string]$OutDir = (Join-Path $env:LOCALAPPDATA 'Darkness2VR\presentmon')
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$pm = Get-Command presentmon -ErrorAction SilentlyContinue
if (-not $pm) {
    $pm = Get-ChildItem "$env:LOCALAPPDATA\Microsoft\WinGet\Packages" -Recurse -Filter presentmon.exe -ErrorAction SilentlyContinue | Select-Object -First 1
}
if (-not $pm) { throw 'PresentMon not found (winget install Intel.PresentMon.Console)' }

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$csv = Join-Path $OutDir "presentmon-$stamp.csv"
$meta = Join-Path $OutDir "presentmon-$stamp.json"

$game = & (Join-Path $PSScriptRoot 'lib\Find-Darkness2Game.ps1')
& (Join-Path $PSScriptRoot 'lib\Set-GameRenderIni.ps1') -GameDir $game.GameDir -Keys @{
    DisableOpenXR = 1; SubmitMode = 0; HookMask = 0; StartupMode = 1
    RenderPipelineTrace = 0; PresentHookLevel = 0
}
& (Join-Path $PSScriptRoot 'deploy-research-build.ps1') -Install -DllOnly | Out-Null

Get-Process DarknessII -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep 2

$pmExe = if ($pm.Source) { $pm.Source } else { $pm.FullName }
$pmArgs = @(
    '--process_name', $ProcessName,
    '--output_file', $csv,
    '--v2_metrics',
    '--no_console_stats'
)
Write-Host "PresentMon: $pmExe $($pmArgs -join ' ')"
$pmProc = Start-Process -FilePath $pmExe -ArgumentList $pmArgs -PassThru -WindowStyle Hidden

Start-Process 'steam://run/67370'
$deadline = (Get-Date).AddMinutes(4)
while (-not (Get-Process DarknessII -ErrorAction SilentlyContinue)) {
    if ((Get-Date) -gt $deadline) { throw 'Game did not start' }
    Start-Sleep 2
}
$gameStart = Get-Date
while (((Get-Date) - $gameStart).TotalSeconds -lt $DurationSec) {
    if (-not (Get-Process DarknessII -ErrorAction SilentlyContinue)) { break }
    Start-Sleep 2
}
$dur = [int]((Get-Date) - $gameStart).TotalSeconds
Get-Process DarknessII -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep 2
if (-not $pmProc.HasExited) { Stop-Process -Id $pmProc.Id -Force -ErrorAction SilentlyContinue }
Start-Sleep -Seconds 4

$rows = 0
$presentRate = $null
if (Test-Path $csv) {
    $lines = Get-Content $csv
    if ($lines.Count -gt 1) { $rows = $lines.Count - 1 }
    $presentCol = 0
    $hdr = ($lines[0] -split ',')
    for ($i = 0; $i -lt $hdr.Count; $i++) {
        if ($hdr[$i] -match 'PresentMode|MsBetweenPresents|msBetweenDisplayChange') { $presentCol = $i }
    }
}
$result = [ordered]@{
    timestamp = $stamp
    durationSec = $dur
    csvPath = $csv
    rowCount = $rows
    processName = $ProcessName
    hookMask = 0
}
$result | ConvertTo-Json | Set-Content $meta
Write-Host "PresentMon capture: rows=$rows duration=${dur}s csv=$csv"
if ($rows -lt 10) { exit 1 }
exit 0
