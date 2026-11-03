# User-authorized UAC broker: ONLY the fixed bounded collector below is run.
# It grants no generic command channel, never launches/changes/kills the game.
$ErrorActionPreference='Stop'
$root=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$lab=Join-Path $root 'test-output/combined-scenes-input-20261103'
New-Item -ItemType Directory -Force $lab | Out-Null
$collector=Join-Path $PSScriptRoot 'Capture-DarknessCombat.ps1'
$stats=Join-Path $PSScriptRoot 'TraceSessionStats.cs'
$pins=@{};foreach($p in $collector,$stats){$pins[$p]=(Get-FileHash -LiteralPath $p).Hash}
$principal=[Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent())
if(!$principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)){throw 'Administrator token required'}
@{ready=$true;pid=$PID;expiresUtc=[DateTime]::UtcNow.AddMinutes(45).ToString('o')} | ConvertTo-Json | Set-Content (Join-Path $lab 'profiler-ready.json')
$deadline=[DateTime]::UtcNow.AddMinutes(45)
try {
 for($trial=0;$trial -lt 2 -and [DateTime]::UtcNow -lt $deadline;$trial++) {
  $request=Join-Path $lab 'profile-request.json'
  while(!(Test-Path -LiteralPath $request) -and [DateTime]::UtcNow -lt $deadline){Start-Sleep -Milliseconds 500}
  if(!(Test-Path -LiteralPath $request)){break}
  $r=Get-Content -LiteralPath $request -Raw | ConvertFrom-Json
  Move-Item -LiteralPath $request -Destination (Join-Path $lab "profile-request-consumed-$trial.json")
  if($r.action -eq 'exit'){break}
  if($r.action -ne 'capture'){throw 'Only capture/exit supported'}
  foreach($p in $collector,$stats){if((Get-FileHash -LiteralPath $p).Hash -ne $pins[$p]){throw 'Collector changed after approval'}}
  $game=@(Get-Process DarknessII -ErrorAction SilentlyContinue)
  if($game.Count -ne 1 -or $game[0].Id -ne [int]$r.gamePid){throw 'Requested game PID changed'}
  $symbols=Join-Path $lab 'symbols'
  & $collector -DurationSec 30 -BaselineSec 10 -ArmDelaySec 0 -SymbolSource $symbols -OutputRoot (Join-Path $lab 'perf') *> (Join-Path $lab "profiler-$trial.log")
  @{finished=$true;trial=$trial;utc=[DateTime]::UtcNow.ToString('o')} | ConvertTo-Json | Set-Content (Join-Path $lab "profiler-finished-$trial.json")
 }
} catch {
 $_ | Out-String | Set-Content (Join-Path $lab 'profiler-error.txt')
} finally {
 @{ready=$false;pid=$PID;utc=[DateTime]::UtcNow.ToString('o')} | ConvertTo-Json | Set-Content (Join-Path $lab 'profiler-ready.json')
}
