param(
    [ValidateRange(30,120)][int]$DurationSec = 45,
    [ValidateRange(5,20)][int]$BaselineSec = 10,
    [ValidateRange(0,60)][int]$ArmDelaySec = 15,
    [string]$GameDir = 'C:\Program Files (x86)\Steam\steamapps\common\Darkness II',
    [string]$OutputRoot = (Join-Path $env:LOCALAPPDATA 'Darkness2VR\perf'),
    [switch]$OutdoorTransition,
    [switch]$AudioCues,
    [string]$SymbolSource = '',
    [string]$LocalVrDir = (Join-Path $env:LOCALAPPDATA 'Darkness2VR'),
    [switch]$Preflight
)

# Keep planning/marking in this collector; no second profiler or game-side timer.
function Get-D2vrPhasePlan([bool]$Outdoor, [int]$Duration, [int]$Baseline) {
    if ($Baseline -lt 5) { throw 'Baseline must be at least 5 seconds.' }
    if ($Outdoor) {
        $combat = $Duration - $Baseline - 15 - 10
        if ($combat -lt 10) { throw 'Outdoor capture must leave at least 10 seconds of combat.' }
        @(
            [pscustomobject]@{id='indoor'; label='D2VR indoor stationary'; seconds=$Baseline; cue=1}
            [pscustomobject]@{id='transition'; label='D2VR exit building'; seconds=15; cue=2}
            [pscustomobject]@{id='outdoor'; label='D2VR outdoor stationary'; seconds=10; cue=3}
            [pscustomobject]@{id='combat'; label='D2VR outdoor movement combat'; seconds=$combat; cue=4}
        )
    } else {
        if ($Duration-$Baseline -lt 10) { throw 'Baseline must leave at least 10 seconds of combat.' }
        @(
            [pscustomobject]@{id='baseline'; label='D2VR baseline stationary'; seconds=$Baseline; cue=1}
            [pscustomobject]@{id='combat'; label='D2VR combat start'; seconds=($Duration-$Baseline); cue=2}
        )
    }
}
function Write-D2vrPhaseMarker([string]$Command, [string]$Instance, [string]$Label) {
    # An external WPR invocation has nonzero latency. Retain BOTH QPC bounds;
    # never pretend its process-return time is the exact ETW marker timestamp.
    $before = [System.Diagnostics.Stopwatch]::GetTimestamp()
    & $Command -marker $Label -instancename $Instance | Out-Null
    $code = $LASTEXITCODE
    $after = [System.Diagnostics.Stopwatch]::GetTimestamp()
    if ($code -ne 0) { throw "ETW phase marker failed ($code): $Label" }
    [pscustomobject]@{markerBeforeQpc=$before; markerAfterQpc=$after}
}
function Wait-D2vrPhase([int]$Seconds, [int]$GamePid, [long]$GameStartTicks, $Presentmon) {
    $deadline=[System.Diagnostics.Stopwatch]::GetTimestamp()+$Seconds*[System.Diagnostics.Stopwatch]::Frequency
    while ([System.Diagnostics.Stopwatch]::GetTimestamp() -lt $deadline) {
        $current=[System.Diagnostics.Process]::GetProcessById($GamePid)
        try {
            if ($current.HasExited -or $current.StartTime.ToUniversalTime().Ticks -ne $GameStartTicks) {
                throw 'Audited game process exited or was replaced during capture.'
            }
        } finally { $current.Dispose() }
        $Presentmon.Refresh()
        if ($Presentmon.HasExited) { throw 'PresentMon exited during a recording phase.' }
        $remaining=($deadline-[System.Diagnostics.Stopwatch]::GetTimestamp())*1000/[System.Diagnostics.Stopwatch]::Frequency
        if ($remaining -gt 0) { Start-Sleep -Milliseconds ([int][Math]::Min(1000,[Math]::Ceiling($remaining))) }
    }
}
function Merge-D2vrLoss($Queried, $Reported) {
    # Session query is authoritative even if stdout later says zero loss.
    $known=@($Queried,$Reported | Where-Object { $null -ne $_ })
    if ($known.Count) { return ($known | Measure-Object -Maximum).Maximum }
    return $null
}
function Invoke-D2vrCue([int]$Count) {
    # Only the phase boundaries make sound, on this helper process. No focus,
    # game input, controller mapping or VR configuration is changed.
    for ($i=0; $i -lt $Count; ++$i) {
        [Console]::Beep(1000,100)
        if ($i+1 -lt $Count) { Start-Sleep -Milliseconds 100 }
    }
}
$ErrorActionPreference = 'Stop'
if ($OutdoorTransition -and -not $PSBoundParameters.ContainsKey('DurationSec')) { $DurationSec=60 }
$phasePlan = @(Get-D2vrPhasePlan ([bool]$OutdoorTransition) $DurationSec $BaselineSec)
$expectedExeHash = 'D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1'
$exe = Join-Path $GameDir 'DarknessII.exe'
if (-not (Test-Path -LiteralPath $exe)) { throw "Missing game executable: $exe" }
$actualExeHash = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
if ($actualExeHash -ne $expectedExeHash) { throw "Game EXE identity changed: $actualExeHash" }
$wpr = (Get-Command wpr.exe -ErrorAction Stop | Select-Object -First 1).Source
$xperf = (Get-Command xperf.exe -ErrorAction Stop | Select-Object -First 1).Source
$presentmon = (Get-Command presentmon.exe -ErrorAction Stop | Select-Object -First 1).Source
$wprStatus = (& $wpr -status 2>&1 | Out-String)
if ($wprStatus -notmatch 'WPR is not recording') {
    throw "Another WPR session exists or status is unknown: $wprStatus"
}
if (Get-Process presentmon -ErrorAction SilentlyContinue) {
    throw 'PresentMon is already running; leaving its session untouched.'
}
# Outdoor stack attribution requires the EXACT measured DLL's symbols. MAIN
# snapshots DLL/PDB/MAP from one link before any other build can overwrite them.
$symbolFiles = @()
if ($OutdoorTransition) {
    if (-not $SymbolSource) { throw 'Outdoor capture requires -SymbolSource with the exact DLL/PDB/MAP build.' }
    foreach ($name in @('d3d9.dll','d3d9.pdb','d3d9.map')) {
        $path = Join-Path $SymbolSource $name
        if (-not (Test-Path -LiteralPath $path)) { throw "Missing measured build artifact: $path" }
        $symbolFiles += $path
    }
    if ((Get-FileHash -LiteralPath $symbolFiles[0] -Algorithm SHA256).Hash -ne
        (Get-FileHash -LiteralPath (Join-Path $GameDir 'd3d9.dll') -Algorithm SHA256).Hash) {
        throw 'Symbol-source DLL does not match the installed DLL; do not substitute another build PDB.'
    }
}
if ($Preflight) {
    Write-Host ("Phases: " + (($phasePlan | ForEach-Object { "$($_.id)=$($_.seconds)s" }) -join ', '))
    Write-Host "PASS: EXE SHA=$actualExeHash; WPR=$wpr; PresentMon=$presentmon; xperf=$xperf; no recorder active."
    return
}
if ($ArmDelaySec) { Start-Sleep -Seconds $ArmDelaySec }
$game = @(Get-Process DarknessII -ErrorAction SilentlyContinue)
if ($game.Count -ne 1 -or $game[0].Path -ne $exe) {
    throw 'Start the audited DarknessII.exe through Steam and load gameplay before capture; no game was launched or changed.'
}
$compositor = @(Get-Process vrcompositor -ErrorAction SilentlyContinue)
$server = @(Get-Process vrserver -ErrorAction SilentlyContinue)
if ($compositor.Count -ne 1 -or $server.Count -ne 1) {
    throw 'SteamVR compositor/server must be running for a headset performance capture.'
}

$stamp = [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ') + '-' + [Guid]::NewGuid().ToString('N').Substring(0,8)
$instance = "D2VRPerf$stamp"
$pmSession = "D2VRPresent$stamp"
if (-not ('D2vrTraceStats' -as [type])) { Add-Type -Path (Join-Path $PSScriptRoot 'TraceSessionStats.cs') }
$out = Join-Path $OutputRoot $stamp
New-Item -ItemType Directory -Path $out -Force | Out-Null
$etl = Join-Path $out 'combat.etl'
$csv = Join-Path $out 'presentmon.csv'
$stdout = Join-Path $out 'presentmon.stdout.txt'
$stderr = Join-Path $out 'presentmon.stderr.txt'
$manifestPath = Join-Path $out 'manifest.json'
$paths = @('DarknessII.exe','d3d9.dll','Darkness2VR.ini','Darkness2VR-diagnostic.ini')
$hashes = [ordered]@{}
foreach ($name in $paths) {
    $path = Join-Path $GameDir $name
    $hashes[$name] = if (Test-Path -LiteralPath $path) {
        (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
    } else { $null }
}
$localHashes=[ordered]@{}
foreach ($name in @('Darkness2VR.ini','last-lighting.ini')) {
    $path=Join-Path $LocalVrDir $name
    $localHashes[$path]=if (Test-Path -LiteralPath $path) { (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash } else { $null }
}
$manifest = [ordered]@{
    status = 'started'; durationSec = $DurationSec; baselineSec = $BaselineSec
    wprInstance = $instance; presentmonSession = $pmSession
    exePath = $exe; hashesBefore = $hashes; hashesAfter = $null
    localIniHashesBefore=$localHashes; localIniHashesAfter=[ordered]@{}
    processIds = [ordered]@{ game=$game[0].Id; vrserver=$server[0].Id
        vrcompositor=$compositor[0].Id; dwm=@(Get-Process dwm -ErrorAction SilentlyContinue | ForEach-Object Id) }
    gameStartUtc = $game[0].StartTime.ToUniversalTime().ToString('o')
    qpcFrequency = [System.Diagnostics.Stopwatch]::Frequency
    startUtc = [DateTime]::UtcNow.ToString('o')
    startQpc = [System.Diagnostics.Stopwatch]::GetTimestamp()
    baselineQpc = $null; combatQpc = $null; endQpc = $null
    mode = $(if ($OutdoorTransition) {'indoor_to_outdoor'} else {'stationary_combat'})
    phases = @(); presentmonScope = $(if ($OutdoorTransition) {'all_processes'} else {'game_pid'})
    phaseMeaning = 'Instructions, not proof of scene or action; inspect focus/world/frame receipts.'
    symbolArtifacts = @()
    etl = $etl; presentmonCsv = $csv; etwLostEvents = $null
    presentmonLostEvents = $null; valid = $false; errors = @()
}
$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
if ($symbolFiles.Count) {
    $symbolOut = Join-Path $out 'symbols'
    New-Item -ItemType Directory -Path $symbolOut | Out-Null
    foreach ($path in $symbolFiles) {
        $dest=Join-Path $symbolOut (Split-Path -Leaf $path)
        Copy-Item -LiteralPath $path -Destination $dest
        $sha=(Get-FileHash -LiteralPath $dest -Algorithm SHA256).Hash
        if ($sha -ne (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash) { throw 'Symbol backup hash mismatch' }
        if ((Split-Path -Leaf $path) -eq 'd3d9.dll' -and $sha -ne $hashes['d3d9.dll']) {
            throw 'Copied symbol-source DLL differs from the measured installed DLL'
        }
        $manifest.symbolArtifacts += [ordered]@{file=(Split-Path -Leaf $path); sha256=$sha}
    }
}
# System-wide WPR owns CPU sampled/precise, stacks and GPU scheduling for ALL
# processes; PresentMon also keeps compositor/DWM rows in outdoor mode.
$manifest.processesBefore = @(Get-Process | Select-Object Id,ProcessName)
$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
$wprStarted = $false
$pm = $null
try {
    $statusNow=(& $wpr -status 2>&1 | Out-String)
    if ($statusNow -notmatch 'WPR is not recording' -or (Get-Process presentmon -ErrorAction SilentlyContinue)) {
        throw 'Recorder ownership changed while arming; leaving other sessions untouched.'
    }
    # CPU.Verbose includes CSwitch/ReadyThread/SampledProfile stacks. GPU.Light
    # adds DxgKrnl without the much larger GPU.Verbose provider set.
    & $wpr -start CPU.Verbose -start GPU.Light -filemode -instancename $instance | Out-File (Join-Path $out 'wpr-start.txt')
    if ($LASTEXITCODE -ne 0) { throw "WPR start failed: $LASTEXITCODE" }
    $wprStarted = $true
    $pmArgs = @('--output_file',('"'+$csv+'"'),
        '--qpc_time_ms','--v2_metrics','--no_console_stats','--timed',"$($DurationSec+30)",
        '--terminate_after_timed','--session_name',$pmSession)
    if (-not $OutdoorTransition) { $pmArgs += @('--process_id',"$($game[0].Id)") }
    $pm = Start-Process -FilePath $presentmon -ArgumentList $pmArgs -PassThru -WindowStyle Hidden `
        -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    Start-Sleep -Seconds 2
    $pm.Refresh()
    if ($pm.HasExited) { throw "PresentMon stopped before capture: $($pm.ExitCode)" }
    foreach ($phase in $phasePlan) {
        $mark = Write-D2vrPhaseMarker $wpr $instance $phase.label
        if ($AudioCues) { Invoke-D2vrCue $phase.cue }
        $phaseReceipt=[ordered]@{
            id=$phase.id; label=$phase.label; seconds=$phase.seconds
            markerBeforeQpc=$mark.markerBeforeQpc; markerAfterQpc=$mark.markerAfterQpc
            startQpc=[System.Diagnostics.Stopwatch]::GetTimestamp(); endQpc=$null; completed=$false
        }
        if ($phase.id -in @('baseline','indoor')) { $manifest.baselineQpc=$phaseReceipt.startQpc }
        if ($phase.id -eq 'combat') { $manifest.combatQpc=$phaseReceipt.startQpc }
        $manifest.phases += $phaseReceipt
        # Four writes in a helper process, one per phase, preserve partial trace
        # boundaries if the game exits. No frame-by-frame synchronous logging.
        $manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
        Write-Host "$($phase.label): $($phase.seconds) seconds."
        Wait-D2vrPhase $phase.seconds $game[0].Id ($game[0].StartTime.ToUniversalTime().Ticks) $pm
        $phaseReceipt.endQpc=[System.Diagnostics.Stopwatch]::GetTimestamp()
        $phaseReceipt.completed=$true
    }
} catch {
    $manifest.errors += $_.Exception.Message
} finally {
    $manifest.endQpc = [System.Diagnostics.Stopwatch]::GetTimestamp()
    if ($manifest.phases.Count -and -not $manifest.phases[-1].endQpc) {
        $manifest.phases[-1].endQpc=$manifest.endQpc
    }
    if ($AudioCues) {
        try { Invoke-D2vrCue 5 } catch { $manifest.errors += "End cue: $($_.Exception.Message)" }
    }
    $manifest.endUtc = [DateTime]::UtcNow.ToString('o')
    $stillGame = @(Get-Process DarknessII -ErrorAction SilentlyContinue)
    $manifest.gameAliveAtCaptureEnd = $stillGame.Count -eq 1 -and $stillGame[0].Id -eq $game[0].Id -and
        $stillGame[0].StartTime.ToUniversalTime().ToString('o') -eq $manifest.gameStartUtc
    if (-not $manifest.gameAliveAtCaptureEnd) { $manifest.errors += 'Game exited during capture' }
    try {
      if ($pm) {
        $pm.Refresh()
        if (-not $pm.HasExited) {
            $pmStats=[D2vrTraceStats]::Query($pmSession)
            $manifest.presentmonSessionStats=$pmStats
            if($pmStats.Error -eq 0) {
                $manifest.presentmonLostEvents=$pmStats.EventsLost
                if($pmStats.LogBuffersLost -or $pmStats.RealTimeBuffersLost) {
                    $manifest.errors += 'PresentMon ETW buffers were lost'
                }
            }
            & $presentmon --terminate_existing_session --session_name $pmSession `
                2>&1 | Out-File (Join-Path $out 'presentmon-stop.txt')
            $manifest.presentmonStopExitCode=$LASTEXITCODE
            if ($LASTEXITCODE -ne 0) { $manifest.errors += 'PresentMon session stop returned nonzero' }
            $null=$pm.WaitForExit(5000)
            $pm.Refresh()
            if (-not $pm.HasExited) {
                $manifest.errors += 'PresentMon did not exit gracefully'
                try { Stop-Process -Id $pm.Id -ErrorAction Stop } catch {
                    $manifest.errors += "PresentMon stop: $($_.Exception.Message)"
                }
            }
        }
        if ($pm.HasExited) {
            $manifest.presentmonExitCode=$pm.ExitCode
            if ($pm.ExitCode -ne 0) { $manifest.errors += "PresentMon exit code: $($pm.ExitCode)" }
        }
      }
    } catch { $manifest.errors += "PresentMon cleanup: $($_.Exception.Message)" }
    # PresentMon cleanup errors must never bypass stopping our named WPR.
    if ($wprStarted) {
      try {
        & $wpr -stop $etl 'DarknessII bounded combat performance capture' -skipPdbGen -instancename $instance `
            2>&1 | Out-File (Join-Path $out 'wpr-stop.txt')
        if ($LASTEXITCODE -ne 0) {
            $manifest.errors += "WPR stop failed: $LASTEXITCODE"
            & $wpr -cancel -instancename $instance 2>&1 | Out-File (Join-Path $out 'wpr-cancel.txt')
        }
      } catch {
        $manifest.errors += "WPR cleanup: $($_.Exception.Message)"
        & $wpr -cancel -instancename $instance 2>&1 | Out-File (Join-Path $out 'wpr-cancel.txt')
      }
    }
    foreach ($name in $paths) {
        $path = Join-Path $GameDir $name
        $hash = if (Test-Path -LiteralPath $path) {
            (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
        } else { $null }
        if ($hash -ne $hashes[$name]) { $manifest.errors += "Installed file changed: $name" }
        if (-not $manifest.hashesAfter) { $manifest.hashesAfter = [ordered]@{} }
        $manifest.hashesAfter[$name] = $hash
    }
    foreach ($path in $localHashes.Keys) {
        $sha=if (Test-Path -LiteralPath $path) { (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash } else { $null }
        $manifest.localIniHashesAfter[$path]=$sha
        if ($sha -ne $localHashes[$path]) { $manifest.errors += "Local INI changed: $path" }
    }
    $gameLog = Join-Path $LocalVrDir 'd3d9.log'
    if (Test-Path -LiteralPath $gameLog) {
        try { Copy-Item -LiteralPath $gameLog -Destination (Join-Path $out 'd3d9.log') }
        catch { $manifest.errors += "Game log copy: $($_.Exception.Message)" }
    }
    $steamRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $GameDir))
    foreach($name in @('vrcompositor.txt','vrserver.txt')) {
        $path=Join-Path $steamRoot "logs\$name"
        if(Test-Path -LiteralPath $path) {
            Get-Content -LiteralPath $path -Tail 3000 | Set-Content -LiteralPath (Join-Path $out $name)
        }
    }
    if (Test-Path -LiteralPath $etl) {
        & $xperf -i $etl -a tracestats -ao (Join-Path $out 'etw-stats.txt') `
            2>&1 | Out-File (Join-Path $out 'etw-stats-command.txt')
        if ($LASTEXITCODE -ne 0) { $manifest.errors += "ETW stats failed: $LASTEXITCODE" }
        & $xperf -i $etl -a cswitch -process -thread -ao (Join-Path $out 'threads.txt') `
            2>&1 | Out-File (Join-Path $out 'threads-command.txt')
        & $xperf -i $etl -a profile -detail -ao (Join-Path $out 'cpu-samples.txt') `
            2>&1 | Out-File (Join-Path $out 'cpu-samples-command.txt')
        $stats = Join-Path $out 'etw-stats.txt'
        if (Test-Path -LiteralPath $stats) {
            $text = Get-Content -LiteralPath $stats -Raw
            $loss = [regex]::Matches($text,'(?im)^\s*Total # Lost Events\s*[:=]\s*(\d+)')
            if ($loss.Count) {
                $manifest.etwLostEvents = @($loss | ForEach-Object { [int]$_.Groups[1].Value } |
                    Measure-Object -Sum)[0].Sum
            }
        }
    }
    $pmText = ''
    foreach ($path in @($stdout,$stderr)) {
        if (Test-Path -LiteralPath $path) { $pmText += Get-Content -LiteralPath $path -Raw }
    }
    $pmLoss = [regex]::Matches($pmText,
        '(?i)(?:lost\s+(\d[\d,]*)\s+(?:ETW\s+)?events?|(\d[\d,]*)\s+(?:ETW\s+)?events?\s+lost)')
    if ($pmLoss.Count) {
        $reportedLoss = @($pmLoss | ForEach-Object {
            $value = if ($_.Groups[1].Success) { $_.Groups[1].Value } else { $_.Groups[2].Value }
            [int]($value -replace ',','') } | Measure-Object -Sum)[0].Sum
        $manifest.presentmonReportedLostEvents=$reportedLoss
        $manifest.presentmonLostEvents=Merge-D2vrLoss $manifest.presentmonLostEvents $reportedLoss
    }
    if (-not (Test-Path -LiteralPath $csv) -or (Get-Item -LiteralPath $csv -ErrorAction SilentlyContinue).Length -lt 100) {
        $manifest.errors += 'PresentMon CSV missing or empty'
    } elseif ($manifest.baselineQpc -and $manifest.combatQpc) {
        $gameRows = @(Import-Csv -LiteralPath $csv | Where-Object ProcessID -eq "$($game[0].Id)")
        foreach ($phase in $manifest.phases) {
            if (-not $phase.completed) { $manifest.errors += "Phase not completed: $($phase.id)" }
            if (-not $phase.endQpc) { $manifest.errors += "Incomplete phase: $($phase.id)"; continue }
            $from=$phase.startQpc*1000/$manifest.qpcFrequency
            $to=$phase.endQpc*1000/$manifest.qpcFrequency
            $phase.rows=@($gameRows | Where-Object {
                [double]$_.CPUStartQPCTime -ge $from -and [double]$_.CPUStartQPCTime -lt $to }).Count
            if ($phase.rows -lt 10) { $manifest.errors += "Phase PresentMon rows incomplete: $($phase.id)" }
        }
        $manifest.baselineRows = ($manifest.phases | Select-Object -First 1).rows
        $manifest.combatRows = ($manifest.phases | Where-Object id -eq 'combat').rows
        if ($manifest.phases.Count -ne $phasePlan.Count) { $manifest.errors += 'Not all scheduled phases were recorded' }
    }
    if ($null -eq $manifest.etwLostEvents) { $manifest.errors += 'ETW lost-event count UNKNOWN; inspect etw-stats.txt' }
    if ($null -eq $manifest.presentmonLostEvents) {
        $manifest.errors += 'PresentMon lost-event count UNKNOWN; inspect PresentMon output'
    }
    if ($manifest.etwLostEvents -gt 0) { $manifest.errors += "ETW lost events: $($manifest.etwLostEvents)" }
    if ($manifest.presentmonLostEvents -gt 0) {
        $manifest.errors += "PresentMon lost events: $($manifest.presentmonLostEvents)"
    }
    $manifest.valid = $manifest.errors.Count -eq 0 -and
        $manifest.etwLostEvents -eq 0 -and $manifest.presentmonLostEvents -eq 0
    $manifest.status = if ($manifest.valid) { 'complete' } else { 'invalid_or_incomplete' }
    $manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
    Write-Host "Capture $($manifest.status): $out"
    if (-not $manifest.valid) { Write-Warning ($manifest.errors -join '; ') }
}
