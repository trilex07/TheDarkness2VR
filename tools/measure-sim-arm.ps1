# Stationary simulator A/B sample: freshly recorded frame timing plus
# per-thread CPU time of the game over the same window. Local lab use only.
param([Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9_-]+$')][string]$Name,[string]$LabDirectory,[ValidateRange(1,60)][double]$Seconds=20)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$lab=if($LabDirectory){$LabDirectory}else{Join-Path $root 'test-output/menu-state-cache-lab-20261105'}
$exe=Join-Path $root 'tools-local/OpenXR-Simulator/bin-x86/openxr_sim_frametime.exe'
$p=Get-Process DarknessII -ErrorAction Stop | Select-Object -First 1
$startTicks=$p.StartTime.ToUniversalTime().Ticks
$log="$env:LOCALAPPDATA\Darkness2VR\d3d9.log"
$tidLine=Select-String -Path $log -Pattern 'VrFrame called by .*tid=(\d+)' | Select-Object -Last 1
$renderTid=[int]$tidLine.Matches[0].Groups[1].Value
$logLinesBefore=(Get-Content $log).Count
function Snap { $h=@{}; foreach($t in (Get-Process -Id $p.Id).Threads){ try{$h[$t.Id]=$t.TotalProcessorTime.TotalMilliseconds}catch{} }; $h }
$capture=Join-Path $lab "$Name.xrft"
$recordJson=Join-Path $lab "$Name-record.json"
$json=Join-Path $lab "$Name-frametime.json"
$summary=Join-Path $lab "$Name-summary.json"
foreach($path in @($capture,$recordJson,$json,$summary)){
 if(Test-Path -LiteralPath $path){throw "Sample already exists: $path; choose a new Name"}
}
$a=Snap; $sw=[Diagnostics.Stopwatch]::StartNew()
# --live-summary ends at the newest ring record, even when that record is old.
# Record skips the historical ring; a stopped renderer cannot reuse old FPS.
& $exe --record --pid $p.Id --seconds $Seconds --out $capture --label $Name --json $recordJson | Out-Null
if($LASTEXITCODE -ne 0){throw 'Simulator recording failed'}
$wall=$sw.Elapsed.TotalMilliseconds; $b=Snap
$current=Get-Process -Id $p.Id -ErrorAction Stop
if($current.StartTime.ToUniversalTime().Ticks -ne $startTicks){throw 'Game process changed during sample'}
$record=Get-Content -LiteralPath $recordJson -Raw | ConvertFrom-Json
if($record.frames -lt 2 -or $record.lost_frames -ne 0){throw 'No complete fresh frame sample (empty or lost records)'}
& $exe --analyze $capture --top 5 --hz 90 --hitch-budgets 2 --json $json | Out-Null
if($LASTEXITCODE -ne 0){throw 'Simulator capture analysis failed'}
$threads=foreach($k in $b.Keys){ if($a.ContainsKey($k)){ [pscustomobject]@{tid=$k;pct=[math]::Round(100*($b[$k]-$a[$k])/$wall,1)} } }
$top=$threads | Sort-Object pct -Descending | Select-Object -First 5
$newLog=Get-Content $log | Select-Object -Skip $logLinesBefore
$perf=$newLog | Where-Object { $_ -match '^\[StereoPerf\]' } | ForEach-Object { if($_ -match 'replay\+history=([0-9.]+).*right\+restore=([0-9.]+)'){ [pscustomobject]@{rh=[double]$matches[1];rr=[double]$matches[2]} } }
$cad=$newLog | Where-Object { $_ -match '^\[XRCadence\]' } | ForEach-Object { if($_ -match 'world-bearing=([0-9.]+)'){[double]$matches[1]} }
$ft=Get-Content $json -Raw | ConvertFrom-Json
if($ft.frames -lt 2 -or $ft.seconds -lt ($Seconds*0.8)){
 throw 'Incomplete fresh frame window; refusing to report FPS'
}
$out=[ordered]@{name=$Name;pid=$p.Id;renderTid=$renderTid;wallMs=[math]::Round($wall);
 capture=$capture;freshFrames=$ft.frames;sampleSeconds=$ft.seconds;
 renderThreadPct=($threads | Where-Object tid -eq $renderTid).pct;topThreads=$top;
 delivered_fps=$ft.delivered_fps;frame_time_ms=$ft.frame_time_ms;over_budget=$ft.over_budget;late_causes=$ft.late_causes;
 replayHistoryMs=@($perf.rh);rightRestoreMs=@($perf.rr);worldBearingPerSec=@($cad)}
$out | ConvertTo-Json -Depth 5 | Set-Content $summary
$out | ConvertTo-Json -Depth 5
