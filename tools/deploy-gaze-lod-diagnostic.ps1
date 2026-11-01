# MAIN-only deployment of a read-only LOD configuration witness.
param([switch]$Install)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$game='C:/Program Files (x86)/Steam/steamapps/common/Darkness II'
$local='C:/Users/Trilex/AppData/Local/Darkness2VR'
$candidate=Join-Path $root 'build/gaze-lod-diagnostic-final/d3d9.dll'
$exe=Join-Path $game 'DarknessII.exe'
$expectedExe='D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1'
$expectedOld='7DB3D6475C8E78E25A793226559504A9B848EC8B7047DD0588E8571398D5FE38'
$expectedNew='148538F92CB37B2AF2D166282972B29FF1C23E46E7D70BDB1787D700719889CF'
$oldManifest=Join-Path $local 'packages/engine-frame-cap-zero-20261101T012443594/rollback/manifest.json'
$oldRestore=Join-Path $local 'packages/engine-frame-cap-zero-20261101T012443594/rollback/Restore.ps1'
function Hash([string]$p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash}
if((Hash $exe) -ne $expectedExe){throw 'Stock EXE mismatch'}
if((Hash (Join-Path $game 'd3d9.dll')) -ne $expectedOld){throw 'Installed DLL changed; re-evaluate'}
if((Hash $candidate) -ne $expectedNew){throw 'Candidate changed; re-evaluate'}
$gate=& (Join-Path $root 'tools/assert-candidate-profile.ps1') -CandidateDll $candidate `
    -RenderIni @((Join-Path $game 'Darkness2VR.ini'),(Join-Path $local 'Darkness2VR.ini')) `
    -DiagnosticIni (Join-Path $game 'Darkness2VR-diagnostic.ini')
if($gate.Flavor -ne 'protected-native'){throw 'Protected-native candidate required'}
$baseline=Get-Content -LiteralPath $oldManifest -Raw | ConvertFrom-Json
if(@($baseline.before).Count -ne 9){throw 'Expected nine protected targets'}
foreach($entry in $baseline.before){if(!(Test-Path -LiteralPath $entry.target)){throw "Missing target $($entry.target)"}}
if(!$Install){Write-Output "PREFLIGHT PASS $expectedNew; no files changed";return}
if(Get-CimInstance Win32_Process | Where-Object {$_.Name -eq 'DarknessII.exe'}){throw 'Close Darkness II before installing'}
$package=Join-Path $local ('packages/gaze-lod-diagnostic-'+(Get-Date -Format 'yyyyMMddTHHmmssfff'))
$rollback=Join-Path $package 'rollback'
New-Item -ItemType Directory -Path $rollback,(Join-Path $package 'candidate') | Out-Null
$before=@();$index=0
foreach($entry in $baseline.before){
    $saved=Join-Path $rollback ('{0:D2}-{1}' -f $index,(Split-Path -Leaf $entry.target));$index++
    $sha=Hash $entry.target
    Copy-Item -LiteralPath $entry.target -Destination $saved
    if((Hash $saved) -ne $sha){throw 'Backup hash mismatch'}
    $before+=@{target=$entry.target;backup=$saved;sha256=$sha}
}
@{game_dir=$game;exe_sha256=$expectedExe;before=$before} | ConvertTo-Json -Depth 6 |
    Set-Content -LiteralPath (Join-Path $rollback 'manifest.json') -Encoding UTF8
Copy-Item -LiteralPath $oldRestore -Destination (Join-Path $rollback 'Restore.ps1')
Copy-Item -LiteralPath $candidate -Destination (Join-Path $package 'candidate/d3d9.dll')
Copy-Item -LiteralPath (Join-Path $root 'build/gaze-lod-diagnostic-final/d3d9.pdb') -Destination (Join-Path $package 'candidate/d3d9.pdb')
Copy-Item -LiteralPath (Join-Path $root 'build/gaze-lod-diagnostic-final/d3d9.map') -Destination (Join-Path $package 'candidate/d3d9.map')
Copy-Item -LiteralPath (Join-Path $root 'src/gaze_lod_diagnostic.inl') -Destination (Join-Path $package 'candidate/gaze_lod_diagnostic.inl')
Copy-Item -LiteralPath (Join-Path $root 'build/gaze-lod-diagnostic-final/src/d3d9.cpp') -Destination (Join-Path $package 'candidate/d3d9.cpp')
Copy-Item -LiteralPath (Join-Path $local 'packages/engine-frame-cap-zero-20261101T012443594/candidate/source.zip') -Destination (Join-Path $package 'candidate/base-source.zip')
@{dll_sha256=$expectedNew;source='installed v11/frame-cap source.zip plus gaze_lod_diagnostic.inl and d3d9.cpp include/call';
  build='build.ps1 -DiagnosticCapture';tests='eye tracking 26/26; weapon wheel 137/137';
  diagnostic='Render/GazeLodDiagnostic=1; configuration and gaze only'} | ConvertTo-Json -Depth 4 |
    Set-Content -LiteralPath (Join-Path $package 'candidate/source.json') -Encoding UTF8
try {
    Copy-Item -LiteralPath $candidate -Destination (Join-Path $game 'd3d9.dll') -Force
    foreach($ini in @((Join-Path $game 'Darkness2VR.ini'),(Join-Path $local 'Darkness2VR.ini'))){
        $bytes=[IO.File]::ReadAllBytes($ini)
        $ascii=[Text.Encoding]::ASCII.GetString($bytes)
        if($ascii -match '(?im)^GazeLodDiagnostic\s*='){throw "Existing GazeLodDiagnostic setting in $ini"}
        if($ascii -notmatch '(?im)^\[Render\]\s*$'){throw "Missing Render section in $ini"}
        $prefix=[Text.Encoding]::ASCII.GetBytes("[Render]`r`n")
        for($i=0;$i -lt $prefix.Length;$i++){if($bytes[$i] -ne $prefix[$i]){throw 'Unexpected INI header'}}
        $insertion=[Text.Encoding]::ASCII.GetBytes("GazeLodDiagnostic=1`r`n")
        $out=[byte[]]::new($bytes.Length+$insertion.Length)
        [Array]::Copy($bytes,0,$out,0,$prefix.Length)
        [Array]::Copy($insertion,0,$out,$prefix.Length,$insertion.Length)
        [Array]::Copy($bytes,$prefix.Length,$out,$prefix.Length+$insertion.Length,$bytes.Length-$prefix.Length)
        [IO.File]::WriteAllBytes($ini,$out)
    }
    if((Hash (Join-Path $game 'd3d9.dll')) -ne $expectedNew){throw 'Installed DLL verification failed'}
    foreach($entry in $before){
        if($entry.target -match 'Darkness2VR\.ini$' -and $entry.target -ne (Join-Path $game 'Darkness2VR-diagnostic.ini')){
            $new=[IO.File]::ReadAllBytes($entry.target);$old=[IO.File]::ReadAllBytes($entry.backup)
            $prefix=[Text.Encoding]::ASCII.GetBytes("[Render]`r`n")
            $insertion=[Text.Encoding]::ASCII.GetBytes("GazeLodDiagnostic=1`r`n")
            if($new.Length -ne $old.Length+$insertion.Length){throw 'INI insertion absent'}
            for($i=0;$i -lt $old.Length;$i++){
                $offset=if($i -lt $prefix.Length){$i}else{$i+$insertion.Length}
                if($new[$offset] -ne $old[$i]){throw 'Existing INI content changed'}
            }
        } elseif($entry.target -ne (Join-Path $game 'd3d9.dll') -and (Hash $entry.target) -ne $entry.sha256){
            throw 'Protected profile/cache changed'
        }
    }
    if((Hash $exe) -ne $expectedExe){throw 'EXE changed'}
} catch {
    & (Join-Path $rollback 'Restore.ps1')
    throw
}
Write-Output "INSTALLED $package DLL=$expectedNew rollback=$rollback; game not launched"
