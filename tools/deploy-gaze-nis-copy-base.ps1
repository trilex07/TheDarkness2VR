param([switch]$Install)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$game='C:/Program Files (x86)/Steam/steamapps/common/Darkness II'
$local='C:/Users/Trilex/AppData/Local/Darkness2VR'
$candidate=Join-Path $root 'build/gaze-lod-diagnostic-final/d3d9.dll'
$exe=Join-Path $game 'DarknessII.exe'
$oldDll='AC6D3568AF0D670F69A8330D08A57B9F3EA4E602783F499A4BEC449848F47E1A'
$newDll='916B9A5151CC25F694A0B967862F3CEFFCC21CE23E8C6D857BF654B164DB7FDF'
$exeHash='D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1'
$previous=Join-Path $local 'packages/gaze-lod-owner-20261101T020523890/rollback'
function Hash([string]$p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash}
function ProfileBytes([byte[]]$source){
    $text=[Text.Encoding]::ASCII.GetString($source)
    $marker='GazeLodDiagnostic=1'
    $offset=$text.IndexOf($marker,[StringComparison]::Ordinal)
    if($offset -lt 0 -or $text.IndexOf($marker,$offset+1,[StringComparison]::Ordinal) -ge 0){throw 'Expected one active gaze diagnostic switch'}
    if($text -match '(?im)^FoveatedCopyBase\s*='){throw 'FoveatedCopyBase already present'}
    $header=[Text.Encoding]::ASCII.GetBytes("[Render]`r`n")
    for($i=0;$i -lt $header.Length;$i++){if($source[$i] -ne $header[$i]){throw 'Unexpected Render header'}}
    $edited=[byte[]]$source.Clone()
    $edited[$offset+$marker.Length-1]=[byte][char]'0'
    $addition=[Text.Encoding]::ASCII.GetBytes("FoveatedCopyBase=1`r`n")
    $result=[byte[]]::new($edited.Length+$addition.Length)
    [Array]::Copy($edited,0,$result,0,$header.Length)
    [Array]::Copy($addition,0,$result,$header.Length,$addition.Length)
    [Array]::Copy($edited,$header.Length,$result,$header.Length+$addition.Length,$edited.Length-$header.Length)
    return ,$result
}
if((Hash $exe) -ne $exeHash){throw 'Stock EXE mismatch'}
if((Hash (Join-Path $game 'd3d9.dll')) -ne $oldDll){throw 'Installed DLL changed'}
if((Hash $candidate) -ne $newDll){throw 'Candidate changed'}
$profiles=@((Join-Path $game 'Darkness2VR.ini'),(Join-Path $local 'Darkness2VR.ini'))
$gate=& (Join-Path $root 'tools/assert-candidate-profile.ps1') -CandidateDll $candidate `
    -RenderIni $profiles -DiagnosticIni (Join-Path $game 'Darkness2VR-diagnostic.ini')
if($gate.Flavor -ne 'protected-native'){throw 'Protected-native candidate required'}
$oldManifest=Get-Content -LiteralPath (Join-Path $previous 'manifest.json') -Raw | ConvertFrom-Json
if(@($oldManifest.before).Count -ne 9){throw 'Expected nine protected targets'}
foreach($ini in $profiles){$null=ProfileBytes([IO.File]::ReadAllBytes($ini))}
if(!$Install){Write-Output "PREFLIGHT PASS $newDll; no files changed";return}
if(Get-Process DarknessII -ErrorAction SilentlyContinue){throw 'Close Darkness II before installation'}
$package=Join-Path $local ('packages/gaze-nis-copy-base-'+(Get-Date -Format 'yyyyMMddTHHmmssfff'))
$rollback=Join-Path $package 'rollback'
New-Item -ItemType Directory -Path $rollback,(Join-Path $package 'candidate') | Out-Null
$before=@();$index=0
foreach($entry in $oldManifest.before){
    $saved=Join-Path $rollback ('{0:D2}-{1}' -f $index,(Split-Path -Leaf $entry.target));$index++
    $sha=Hash $entry.target
    Copy-Item -LiteralPath $entry.target -Destination $saved
    if((Hash $saved) -ne $sha){throw 'Backup verification failed'}
    $before+=@{target=$entry.target;backup=$saved;sha256=$sha}
}
@{game_dir=$game;exe_sha256=$exeHash;before=$before} | ConvertTo-Json -Depth 6 |
    Set-Content -LiteralPath (Join-Path $rollback 'manifest.json') -Encoding UTF8
Copy-Item -LiteralPath (Join-Path $previous 'Restore.ps1') -Destination (Join-Path $rollback 'Restore.ps1')
foreach($name in @('d3d9.dll','d3d9.pdb','d3d9.map')){
    Copy-Item -LiteralPath (Join-Path $root "build/gaze-lod-diagnostic-final/$name") -Destination (Join-Path $package "candidate/$name")
}
foreach($name in @('d3d9.cpp','nis_eye.inl','gaze_nis_copy_base.inl','gaze_lod_diagnostic.inl','gaze_lod_owner_diagnostic.inl','native_aim.inl')){
    Copy-Item -LiteralPath (Join-Path $root "build/gaze-lod-diagnostic-final/src/$name") -Destination (Join-Path $package "candidate/$name")
}
Copy-Item -LiteralPath (Join-Path $root 'test/gaze_nis_copy_base_test.cpp') -Destination (Join-Path $package 'candidate/gaze_nis_copy_base_test.cpp')
Copy-Item -LiteralPath (Join-Path $local 'packages/gaze-lod-diagnostic-20261101T015029208/candidate/base-source.zip') -Destination (Join-Path $package 'candidate/base-source.zip')
@{dll_sha256=$newDll;source='base-source.zip plus candidate d3d9.cpp, nis_eye.inl, gaze_nis_copy_base.inl and native owner diagnostic';
  build='build.ps1 -DiagnosticCapture';test='D3D11 WARP outsideDiff=0 insideDiff=48';
  active_profile='Render/FoveatedCopyBase=1; Render/GazeLodDiagnostic=0';
  limitation='NIS GPU and proxy CPU work only; native engine LOD unchanged'} | ConvertTo-Json -Depth 4 |
    Set-Content -LiteralPath (Join-Path $package 'candidate/source.json') -Encoding UTF8
try {
    Copy-Item -LiteralPath $candidate -Destination (Join-Path $game 'd3d9.dll') -Force
    foreach($ini in $profiles){[IO.File]::WriteAllBytes($ini,(ProfileBytes([IO.File]::ReadAllBytes($ini))))}
    if((Hash (Join-Path $game 'd3d9.dll')) -ne $newDll){throw 'DLL install verification failed'}
    foreach($entry in $before){
        if($entry.target -eq (Join-Path $game 'd3d9.dll')){continue}
        if($profiles -contains $entry.target){
            $expected=ProfileBytes([IO.File]::ReadAllBytes($entry.backup))
            $actual=[IO.File]::ReadAllBytes($entry.target)
            if($actual.Length -ne $expected.Length -or [Convert]::ToHexString($actual) -ne [Convert]::ToHexString($expected)){
                throw "Profile content mismatch: $($entry.target)"
            }
        } elseif((Hash $entry.target) -ne $entry.sha256){throw "Protected target changed: $($entry.target)"}
    }
    if((Hash $exe) -ne $exeHash){throw 'EXE changed'}
} catch {
    & (Join-Path $rollback 'Restore.ps1')
    throw
}
Write-Output "INSTALLED $package DLL=$newDll rollback=$rollback; game not launched"
