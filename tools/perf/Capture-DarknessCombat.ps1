param(
    [ValidateRange(30,60)][int]$DurationSec = 45,
    [ValidateRange(5,20)][int]$BaselineSec = 10,
    [ValidateRange(0,60)][int]$ArmDelaySec = 15,
    [string]$GameDir = 'C:\Program Files (x86)\Steam\steamapps\common\Darkness II',
    [string]$OutputRoot = (Join-Path $env:LOCALAPPDATA 'Darkness2VR\perf'),
    [switch]$Preflight
)

$ErrorActionPreference = 'Stop'
if ($BaselineSec -ge $DurationSec-10) { throw 'Baseline must leave at least 10 seconds of combat.' }
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
if ($Preflight) {
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

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$instance = "D2VRPerf$stamp"
$pmSession = "D2VRPresent$stamp"
Add-Type -Path (Join-Path $PSScriptRoot 'TraceSessionStats.cs')
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
$manifest = [ordered]@{
    status = 'started'; durationSec = $DurationSec; baselineSec = $BaselineSec
    wprInstance = $instance; presentmonSession = $pmSession
    exePath = $exe; hashesBefore = $hashes; hashesAfter = $null
    processIds = [ordered]@{ game=$game[0].Id; vrserver=$server[0].Id
        vrcompositor=$compositor[0].Id; dwm=@(Get-Process dwm -ErrorAction SilentlyContinue | ForEach-Object Id) }
    qpcFrequency = [System.Diagnostics.Stopwatch]::Frequency
    startUtc = [DateTime]::UtcNow.ToString('o')
    startQpc = [System.Diagnostics.Stopwatch]::GetTimestamp()
    baselineQpc = $null; combatQpc = $null; endQpc = $null
    etl = $etl; presentmonCsv = $csv; etwLostEvents = $null
    presentmonLostEvents = $null; valid = $false; errors = @()
}
$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
$wprStarted = $false
$pm = $null
try {
    # CPU.Verbose includes CSwitch/ReadyThread/SampledProfile stacks. GPU.Light
    # adds DxgKrnl without the much larger GPU.Verbose provider set.
    & $wpr -start CPU.Verbose -start GPU.Light -filemode -instancename $instance | Out-File (Join-Path $out 'wpr-start.txt')
    if ($LASTEXITCODE -ne 0) { throw "WPR start failed: $LASTEXITCODE" }
    $wprStarted = $true
    $pmArgs = @('--process_id',"$($game[0].Id)",'--output_file',('"'+$csv+'"'),
        '--qpc_time_ms','--v2_metrics','--no_console_stats','--timed',"$($DurationSec+10)",
        '--terminate_after_timed','--session_name',$pmSession)
    $pm = Start-Process -FilePath $presentmon -ArgumentList $pmArgs -PassThru -WindowStyle Hidden `
        -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    Start-Sleep -Seconds 2
    $pm.Refresh()
    if ($pm.HasExited) { throw "PresentMon stopped before capture: $($pm.ExitCode)" }
    & $wpr -marker 'D2VR baseline stationary' -instancename $instance | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Baseline ETW mark failed.' }
    $manifest.baselineQpc = [System.Diagnostics.Stopwatch]::GetTimestamp()
    Write-Host "Capturing $BaselineSec seconds of stationary gameplay. Combat begins after the baseline marker."
    Start-Sleep -Seconds ($BaselineSec-2)
    & $wpr -marker 'D2VR combat start' -instancename $instance | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Combat ETW mark failed.' }
    $manifest.combatQpc = [System.Diagnostics.Stopwatch]::GetTimestamp()
    Write-Host 'Combat marker recorded.'
    Start-Sleep -Seconds ($DurationSec-$BaselineSec)
} catch {
    $manifest.errors += $_.Exception.Message
} finally {
    $manifest.endQpc = [System.Diagnostics.Stopwatch]::GetTimestamp()
    $manifest.endUtc = [DateTime]::UtcNow.ToString('o')
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
            Wait-Process -Id $pm.Id -Timeout 5 -ErrorAction SilentlyContinue
            $pm.Refresh()
            if (-not $pm.HasExited) {
                $manifest.errors += 'PresentMon did not exit gracefully'
                try { Stop-Process -Id $pm.Id -ErrorAction Stop } catch {
                    $manifest.errors += "PresentMon stop: $($_.Exception.Message)"
                }
            }
        }
    }
    if ($wprStarted) {
        & $wpr -stop $etl 'DarknessII bounded combat performance capture' -skipPdbGen -instancename $instance `
            2>&1 | Out-File (Join-Path $out 'wpr-stop.txt')
        if ($LASTEXITCODE -ne 0) {
            $manifest.errors += "WPR stop failed: $LASTEXITCODE"
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
    $stillGame = @(Get-Process DarknessII -ErrorAction SilentlyContinue)
    if ($stillGame.Count -ne 1 -or $stillGame[0].Id -ne $game[0].Id) {
        $manifest.errors += 'Game exited or PID changed during capture'
    }
    $gameLog = Join-Path $env:LOCALAPPDATA 'Darkness2VR\d3d9.log'
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
            $loss = [regex]::Matches($text,'(?im)^\s*(?:Events Lost|Lost Events|EventsLost)\s*[:=]\s*(\d+)')
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
        $manifest.presentmonLostEvents = @($pmLoss | ForEach-Object {
            $value = if ($_.Groups[1].Success) { $_.Groups[1].Value } else { $_.Groups[2].Value }
            [int]($value -replace ',','') } | Measure-Object -Sum)[0].Sum
    }
    if (-not (Test-Path -LiteralPath $csv) -or (Get-Item -LiteralPath $csv -ErrorAction SilentlyContinue).Length -lt 100) {
        $manifest.errors += 'PresentMon CSV missing or empty'
    } elseif ($manifest.baselineQpc -and $manifest.combatQpc) {
        $gameRows = @(Import-Csv -LiteralPath $csv | Where-Object ProcessID -eq "$($game[0].Id)")
        $baselineMs = $manifest.baselineQpc*1000/$manifest.qpcFrequency
        $combatMs = $manifest.combatQpc*1000/$manifest.qpcFrequency
        $endMs = $manifest.endQpc*1000/$manifest.qpcFrequency
        $manifest.baselineRows = @($gameRows | Where-Object {
            [double]$_.CPUStartTime -ge $baselineMs -and [double]$_.CPUStartTime -lt $combatMs }).Count
        $manifest.combatRows = @($gameRows | Where-Object {
            [double]$_.CPUStartTime -ge $combatMs -and [double]$_.CPUStartTime -le $endMs }).Count
        if ($manifest.baselineRows -lt 10 -or $manifest.combatRows -lt 10) {
            $manifest.errors += 'Baseline/combat PresentMon rows incomplete or QPC clocks do not align'
        }
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
