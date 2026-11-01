param([switch]$Install)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$game='C:/Program Files (x86)/Steam/steamapps/common/Darkness II'
$local='C:/Users/Trilex/AppData/Local/Darkness2VR'
$currentPackage=Join-Path $local 'packages/gaze-lod-debug-20261101T111242901'
$stable=Join-Path $local 'packages/gaze-dot-r2-20261101T031753160/candidate/d3d9.dll'
$currentHash='C398F251E9BE1556292E7683308D55E49A1D3B61C3A415669DF9BEBEC9EC1CB1'
$stableHash='E6F49A7E9E520D24502B9F5ABAFB4E0DAE6A9D7DDC0ACDE86486DAD4128DB1BD'
$exeHash='D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1'
function Hash([string]$path){(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
$exe=Join-Path $game 'DarknessII.exe'
$target=Join-Path $game 'd3d9.dll'
$gameIni=Join-Path $game 'Darkness2VR.ini'
if((Hash $exe) -ne $exeHash){throw 'Stock EXE mismatch'}
if((Hash $target) -ne $currentHash){throw 'Installed LOD diagnostic DLL changed'}
if((Hash $stable) -ne $stableHash){throw 'Stable candidate DLL changed'}
$bytes=[IO.File]::ReadAllBytes($gameIni)
$text=[Text.Encoding]::ASCII.GetString($bytes)
$match=[regex]::Match($text,'(?m)^GazeLodRuntimeObserver=1\r?\n')
if(!$match.Success -or ([regex]::Matches($text,'(?m)^GazeLodRuntimeObserver=1\r?\n')).Count -ne 1){throw 'Expected exactly one LOD observer setting'}
$prior=Join-Path $currentPackage 'rollback'
$manifest=Get-Content -LiteralPath (Join-Path $prior 'manifest.json') -Raw | ConvertFrom-Json
if(@($manifest.before).Count -ne 9){throw 'Expected nine protected targets'}
foreach($entry in $manifest.before){if(!(Test-Path -LiteralPath $entry.target -PathType Leaf)){throw "Missing protected target: $($entry.target)"}}
if(Get-Process DarknessII -ErrorAction SilentlyContinue){throw 'Close Darkness II before restoring stable DLL'}
if(!$Install){Write-Output "PREFLIGHT PASS stable=$stableHash; no files changed";return}
$package=Join-Path $local ('packages/gaze-lod-observer-revert-'+(Get-Date -Format 'yyyyMMddTHHmmssfff'))
$rollback=Join-Path $package 'rollback'
New-Item -ItemType Directory -Path $rollback,(Join-Path $package 'candidate') | Out-Null
$before=@();$index=0
foreach($entry in $manifest.before){
    $saved=Join-Path $rollback ('{0:D2}-{1}' -f $index,(Split-Path -Leaf $entry.target));$index++
    $sha=Hash $entry.target
    Copy-Item -LiteralPath $entry.target -Destination $saved
    if((Hash $saved) -ne $sha){throw "Backup verification failed: $saved"}
    $before+=@{target=$entry.target;backup=$saved;sha256=$sha}
}
@{game_dir=$game;exe_sha256=$exeHash;before=$before} | ConvertTo-Json -Depth 6 |
    Set-Content -LiteralPath (Join-Path $rollback 'manifest.json') -Encoding UTF8
Copy-Item -LiteralPath (Join-Path $prior 'Restore.ps1') -Destination (Join-Path $rollback 'Restore.ps1')
Copy-Item -LiteralPath $stable -Destination (Join-Path $package 'candidate/d3d9.dll')
@{dll_sha256=$stableHash;source_commit='70f4e28e7fea44c97ff7a478cfb93b15d2318d04';
  reason='Read-only LOD observer refused occupied debug registers; keep known stable DLL and current caches'} |
    ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $package 'candidate/source.json') -Encoding UTF8
try {
    Copy-Item -LiteralPath $stable -Destination $target -Force
    $restored=New-Object byte[] ($bytes.Length-$match.Length)
    [Array]::Copy($bytes,0,$restored,0,$match.Index)
    [Array]::Copy($bytes,$match.Index+$match.Length,$restored,$match.Index,$bytes.Length-$match.Index-$match.Length)
    [IO.File]::WriteAllBytes($gameIni,$restored)
    if((Hash $target) -ne $stableHash){throw 'Stable DLL verification failed'}
    if([IO.File]::ReadAllText($gameIni) -match '(?im)^GazeLodRuntimeObserver\s*='){throw 'Observer setting remained'}
    foreach($entry in $before){
        if($entry.target -in @($target,$gameIni)){continue}
        if((Hash $entry.target) -ne $entry.sha256){throw "Protected target changed: $($entry.target)"}
    }
    if((Hash $exe) -ne $exeHash){throw 'EXE changed after restoration'}
} catch {
    & (Join-Path $rollback 'Restore.ps1')
    throw
}
Write-Output "RESTORED $package DLL=$stableHash rollback=$rollback; caches retained; game not launched"
