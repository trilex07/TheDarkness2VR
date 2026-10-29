param(
    [string]$Stage,
    [string]$SourceCommit,
    [string]$GameDir = 'C:\Program Files (x86)\Steam\steamapps\common\Darkness II',
    [string]$Backup,
    [switch]$Rollback,
    [switch]$NoCurrentPointer
)
$ErrorActionPreference = 'Stop'
function Hash([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    $algorithm = [Security.Cryptography.SHA256]::Create()
    try { ([BitConverter]::ToString($algorithm.ComputeHash($stream))).Replace('-','').ToLowerInvariant() }
    finally { $algorithm.Dispose(); $stream.Dispose() }
}
function Assert-Closed {
    $running = @(Get-CimInstance Win32_Process | Where-Object { $_.Name -match '^(DarknessII|ChernobylGame|ChernobylGame-Win64-Shipping|Singularity)\.exe$' })
    if ($running.Count) { throw ('Game is running: ' + (($running | ForEach-Object { $_.Name+' PID '+$_.ProcessId }) -join ', ')) }
}
Assert-Closed
$allowed = @('B.Font','H.Font','H.Misc','H.Misc_en','B.Misc_en','F.Misc_en') | ForEach-Object { $_+'.toc'; $_+'.cache' }
if ($Rollback) {
    if (-not $Backup) { throw 'Backup required' }
    $m = Get-Content -LiteralPath (Join-Path $Backup 'manifest.json') -Raw | ConvertFrom-Json
    if ($m.gameDir -ne $GameDir) { throw 'Backup game path mismatch' }
    foreach ($e in $m.preserved | Where-Object { (Split-Path -Leaf $_.path) -ieq 'DarknessII.exe' }) {
        if ((Hash $e.path) -ne $e.sha256) { throw 'Game EXE changed since deployment' }
    }
    foreach ($e in $m.files) {
        if ($e.name -notin $allowed) { throw 'Unexpected rollback target' }
        if ((Hash (Join-Path $Backup $e.name)) -ne $e.beforeSha256) { throw ('Backup hash mismatch: '+$e.name) }
        $current = Hash (Join-Path $GameDir ('Cache.Windows\'+$e.name))
        if ($current -ne $e.afterSha256 -and $current -ne $e.beforeSha256) { throw ('Changed since deployment: '+$e.name) }
    }
    foreach ($e in $m.files) {
        $target = Join-Path $GameDir ('Cache.Windows\'+$e.name)
        Copy-Item -LiteralPath (Join-Path $Backup $e.name) -Destination $target -Force
        if ((Hash $target) -ne $e.beforeSha256) { throw ('Restore readback mismatch: '+$e.name) }
    }
    'RESTORED_ALL_POLISH_ARCHIVE_HASHES_MATCH; VR DLL and INIs unchanged'
    exit 0
}
if (-not $Stage -or -not $SourceCommit) { throw 'Stage and SourceCommit required' }
$report = Get-Content -LiteralPath (Join-Path $Stage 'build-report.json') -Raw | ConvertFrom-Json
if (-not $report.audio -or -not $report.text -or @($report.fonts).Count -ne 6) { throw 'Incomplete pack' }
$candidate = Join-Path $Stage 'candidate'
$names = @(Get-ChildItem -LiteralPath $candidate -File | ForEach-Object Name)
if ($names.Count -ne $allowed.Count -or @($names | Where-Object { $_ -notin $allowed }).Count) { throw 'Unexpected candidate files' }
$guardPaths = @('d3d9.dll','DarknessII.exe','Cache.Windows\B.Texture_en.toc','Cache.Windows\B.Texture_en.cache','Cache.Windows\B.Misc.toc','Cache.Windows\B.Misc.cache') | ForEach-Object { Join-Path $GameDir $_ }
$guardPaths += @(Get-ChildItem -LiteralPath $GameDir -Filter '*.ini' -File | ForEach-Object FullName)
$guardPaths += @((Join-Path $env:LOCALAPPDATA 'Darkness2VR\Darkness2VR.ini'), (Join-Path $env:LOCALAPPDATA 'Darkness2VR\runtime\last-lighting-profile.ini')) | Where-Object { Test-Path -LiteralPath $_ }
$guards = @($guardPaths | Sort-Object -Unique | ForEach-Object { [pscustomobject]@{ path=$_; sha256=(Hash $_) } })
$files = foreach ($name in $allowed) {
    $target = Join-Path $GameDir ('Cache.Windows\'+$name)
    $before = Hash $target
    if ($before -ne (Hash (Join-Path $Stage ('baseline\'+$name)))) { throw ('Live archive differs from audited baseline: '+$name) }
    $after = Hash (Join-Path $candidate $name)
    if ($after -ne $report.files.$name) { throw ('Candidate differs from build manifest: '+$name) }
    [pscustomobject]@{ name=$name; beforeSha256=$before; afterSha256=$after }
}
if (-not $Backup) { $Backup = Join-Path $env:LOCALAPPDATA ('Darkness2VR\backups\polish-format-repair-'+(Get-Date -Format 'yyyyMMddTHHmmss')) }
if (Test-Path -LiteralPath $Backup) { throw 'Backup directory already exists' }
New-Item -ItemType Directory -Path $Backup | Out-Null
foreach ($e in $files) {
    Copy-Item -LiteralPath (Join-Path $GameDir ('Cache.Windows\'+$e.name)) -Destination (Join-Path $Backup $e.name)
    if ((Hash (Join-Path $Backup $e.name)) -ne $e.beforeSha256) { throw ('Backup verification failed: '+$e.name) }
}
New-Item -ItemType Directory -Path (Join-Path $Backup 'preserved') | Out-Null
$i=0
foreach ($e in $guards) {
    $copy = Join-Path $Backup ('preserved\'+$i+'-'+(Split-Path -Leaf $e.path))
    Copy-Item -LiteralPath $e.path -Destination $copy
    if ((Hash $copy) -ne $e.sha256) { throw ('Protected backup mismatch: '+$e.path) }
    $i++
}
$manifest = [ordered]@{ state='BACKED_UP'; sourceCommit=$SourceCommit; gameDir=$GameDir; stage=$Stage; files=@($files); preserved=@($guards); buildReport=$report }
$manifest | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $Backup 'manifest.json') -Encoding utf8
Copy-Item -LiteralPath $PSCommandPath -Destination (Join-Path $Backup 'restore.ps1')
'& "$PSScriptRoot\restore.ps1" -Rollback -Backup $PSScriptRoot' | Set-Content -LiteralPath (Join-Path $Backup 'rollback.ps1') -Encoding utf8
$installed = @()
try {
    Assert-Closed
    foreach ($e in $files) {
        $target = Join-Path $GameDir ('Cache.Windows\'+$e.name)
        if ((Hash $target) -ne $e.beforeSha256) { throw ('Live archive changed before install: '+$e.name) }
        $installed += $e
        Copy-Item -LiteralPath (Join-Path $candidate $e.name) -Destination $target -Force
        if ((Hash $target) -ne $e.afterSha256) { throw ('Installed readback mismatch: '+$e.name) }
    }
    foreach ($e in $guards) {
        if ((Hash $e.path) -ne $e.sha256) { throw ('Protected VR file changed: '+$e.path) }
    }
    $manifest.state='DEPLOYED_OFFLINE_VERIFIED_NOT_HEADSET_TESTED'
} catch {
    foreach ($e in $installed) {
        $target = Join-Path $GameDir ('Cache.Windows\'+$e.name)
        Copy-Item -LiteralPath (Join-Path $Backup $e.name) -Destination $target -Force
        if ((Hash $target) -ne $e.beforeSha256) { throw ('AUTOMATIC ROLLBACK FAILED: '+$e.name) }
    }
    $manifest.state='INSTALL_FAILED_RESTORED'
    $manifest | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $Backup 'manifest.json') -Encoding utf8
    throw
}
$manifest | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $Backup 'manifest.json') -Encoding utf8
if (-not $NoCurrentPointer) { Set-Content -LiteralPath (Join-Path $env:LOCALAPPDATA 'Darkness2VR\runtime\polish-repair-current.txt') -Value $Backup }
Write-Output ('DEPLOYED; rollback: '+(Join-Path $Backup 'rollback.ps1'))
Write-Output ('Preserved DLL SHA256: '+(Hash (Join-Path $GameDir 'd3d9.dll')))
