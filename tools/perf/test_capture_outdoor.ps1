# Offline: execute the collector's real helper bodies without its main script.
$ErrorActionPreference='Stop'
$source=Join-Path $PSScriptRoot 'Capture-DarknessCombat.ps1'
$tokens=$null; $parseErrors=$null
$ast=[System.Management.Automation.Language.Parser]::ParseFile($source,[ref]$tokens,[ref]$parseErrors)
if ($parseErrors.Count) { throw ($parseErrors | Out-String) }
$ast.FindAll({param($n) $n -is [System.Management.Automation.Language.FunctionDefinitionAst]},$false) |
    ForEach-Object { Invoke-Expression $_.Extent.Text }
$checks=0
function Check([bool]$Condition,[string]$Why) {
    if (-not $Condition) { throw $Why }
    $script:checks++
}
$plan=@(Get-D2vrPhasePlan $true 60 10)
Check (($plan.id -join ',') -eq 'indoor,transition,outdoor,combat') 'Four phase order'
Check (($plan.seconds -join ',') -eq '10,15,10,25') 'One 60-second schedule'
Check (($plan.cue -join ',') -eq '1,2,3,4') 'Audible instructions stay deterministic'
$legacy=@(Get-D2vrPhasePlan $false 45 10)
Check (($legacy.seconds -join ',') -eq '10,35') 'Legacy stationary/combat preserved'
$threw=$false
try { Get-D2vrPhasePlan $true 40 10 | Out-Null } catch { $threw=$true }
Check $threw 'Reject missing outdoor combat time'
$threw=$false
try { Get-D2vrPhasePlan $false 15 10 | Out-Null } catch { $threw=$true }
Check $threw 'Reject incomplete legacy schedule'
Check ((Merge-D2vrLoss 7 0) -eq 7) 'stdout zero must not erase queried event loss'
Check ((Merge-D2vrLoss 0 9) -eq 9) 'Reported loss retained'
Check ((Merge-D2vrLoss $null 0) -eq 0) 'Known zero distinguished from UNKNOWN'
Check ($null -eq (Merge-D2vrLoss $null $null)) 'Missing counters remain UNKNOWN'
$script:wprArgs=@();$script:wprCode=0
function Mock-Wpr { $script:wprArgs=@($args); $global:LASTEXITCODE=$script:wprCode }
$m=Write-D2vrPhaseMarker Mock-Wpr own-session 'D2VR exit building'
Check (($script:wprArgs -join '|') -eq '-marker|D2VR exit building|-instancename|own-session') 'Only owned named marker'
Check ($m.markerBeforeQpc -le $m.markerAfterQpc) 'QPC brackets contain external marker invocation'
$script:wprCode=5; $threw=$false
try { Write-D2vrPhaseMarker Mock-Wpr own-session fail | Out-Null } catch { $threw=$true }
Check $threw 'Failed ETW mark stops phase rather than fabricating time'
$deadPm=[pscustomobject]@{HasExited=$true}
$deadPm | Add-Member ScriptMethod Refresh {}
$own=[System.Diagnostics.Process]::GetCurrentProcess()
$threw=$false
try { Wait-D2vrPhase 1 $own.Id $own.StartTime.ToUniversalTime().Ticks $deadPm } catch { $threw=$true }
Check $threw 'Premature recorder exit detected'
$threw=$false
try { Wait-D2vrPhase 1 $own.Id 1 $deadPm } catch { $threw=$true }
Check $threw 'Replaced game process identity rejected'
$livePm=[pscustomobject]@{HasExited=$false}
$livePm | Add-Member ScriptMethod Refresh {}
$begin=[System.Diagnostics.Stopwatch]::GetTimestamp()
Wait-D2vrPhase 1 $own.Id ($own.StartTime.ToUniversalTime().Ticks) $livePm
$elapsed=([System.Diagnostics.Stopwatch]::GetTimestamp()-$begin)/[System.Diagnostics.Stopwatch]::Frequency
Check ($elapsed -ge 1 -and $elapsed -lt 3) 'Valid phase completes on QPC deadline without recorder input'
$own.Dispose()
Write-Host "PASS: $checks real collector helper checks; no recorder or Steam session started."
