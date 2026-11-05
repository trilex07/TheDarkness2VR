# Stationary simulator A/B sample: frame timing (simulator live summary) plus
# per-thread CPU time of the game over the same window. Local lab use only.
param([Parameter(Mandatory)][string]$Name,[string]$LabDirectory,[double]$Seconds=20)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$lab=if($LabDirectory){$LabDirectory}else{Join-Path $root 'test-output/menu-state-cache-lab-20261105'}
$exe=Join-Path $root 'tools-local/OpenXR-Simulator/bin-x86/openxr_sim_frametime.exe'
$p=Get-Process DarknessII -ErrorAction Stop | Select-Object -First 1
$log="$env:LOCALAPPDATA\Darkness2VR\d3d9.log"
$tidLine=Select-String -Path $log -Pattern 'VrFrame called by .*tid=(\d+)' | Select-Object -Last 1
$renderTid=[int]$tidLine.Matches[0].Groups[1].Value
$logLinesBefore=(Get-Content $log).Count
function Snap { $h=@{}; foreach($t in (Get-Process -Id $p.Id).Threads){ try{$h[$t.Id]=$t.TotalProcessorTime.TotalMilliseconds}catch{} }; $h }
$a=Snap; $sw=[Diagnostics.Stopwatch]::StartNew()
Start-Sleep -Milliseconds ([int]($Seconds*1000))   # measurement window
$wall=$sw.Elapsed.TotalMilliseconds; $b=Snap
# live-summary analyses the most recent $Seconds of the runtime's frame ring,
# i.e. the same window the thread CPU deltas cover.
$json=Join-Path $lab "$Name-frametime.json"
& $exe --live-summary --pid $p.Id --seconds $Seconds --top 5 --hz 90 --hitch-budgets 2 --json $json | Out-Null
$threads=foreach($k in $b.Keys){ if($a.ContainsKey($k)){ [pscustomobject]@{tid=$k;pct=[math]::Round(100*($b[$k]-$a[$k])/$wall,1)} } }
$top=$threads | Sort-Object pct -Descending | Select-Object -First 5
$newLog=Get-Content $log | Select-Object -Skip $logLinesBefore
$perf=$newLog | Where-Object { $_ -match '^\[StereoPerf\]' } | ForEach-Object { if($_ -match 'replay\+history=([0-9.]+).*right\+restore=([0-9.]+)'){ [pscustomobject]@{rh=[double]$matches[1];rr=[double]$matches[2]} } }
$cad=$newLog | Where-Object { $_ -match '^\[XRCadence\]' } | ForEach-Object { if($_ -match 'world-bearing=([0-9.]+)'){[double]$matches[1]} }
$ft=Get-Content $json -Raw | ConvertFrom-Json
$out=[ordered]@{name=$Name;pid=$p.Id;renderTid=$renderTid;wallMs=[math]::Round($wall);
 renderThreadPct=($threads | Where-Object tid -eq $renderTid).pct;topThreads=$top;
 delivered_fps=$ft.delivered_fps;frame_time_ms=$ft.frame_time_ms;over_budget=$ft.over_budget;late_causes=$ft.late_causes;
 replayHistoryMs=@($perf.rh);rightRestoreMs=@($perf.rr);worldBearingPerSec=@($cad)}
$out | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $lab "$Name-summary.json")
$out | ConvertTo-Json -Depth 5
