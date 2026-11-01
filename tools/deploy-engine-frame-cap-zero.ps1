# MAIN-only, explicitly authorized installation over eye-tracking v11.
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$candidate=Join-Path $root 'build/frame-cap-v11'
$game='C:/Program Files (x86)/Steam/steamapps/common/Darkness II'
$local=Join-Path $env:LOCALAPPDATA 'Darkness2VR'
$previous=Join-Path $local 'packages/eye-tracking-v11-20261101T010419189/rollback'
function Hash($p) { (Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash }
function Assert-Free { if(Get-Process DarknessII -ErrorAction SilentlyContinue){throw 'Close Darkness II first'} }
Assert-Free
$exeHash='D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1'
if((Hash "$game/DarknessII.exe") -ne $exeHash){throw 'EXE changed'}
if((Hash "$game/d3d9.dll") -ne 'AE52F6F4557120ACA32D7A5FDEA61AE8D31EDD6B82F2B3262E1B37C3A0122D1C'){throw 'Installed v11 changed'}
$dllHash=Hash "$candidate/d3d9.dll"
$profiles=@("$game/Darkness2VR.ini","$local/Darkness2VR.ini")
$gate=& "$root/tools/assert-candidate-profile.ps1" -CandidateDll "$candidate/d3d9.dll" -RenderIni $profiles -DiagnosticIni "$game/Darkness2VR-diagnostic.ini"
if($gate.Flavor -ne 'protected-native'){throw 'Wrong candidate flavor'}
$package=Join-Path $local ('packages/engine-frame-cap-zero-'+(Get-Date -Format 'yyyyMMddTHHmmssfff'))
$rollback=Join-Path $package 'rollback'
New-Item -ItemType Directory -Path $rollback,"$package/candidate" | Out-Null
$old=Get-Content -LiteralPath "$previous/manifest.json" -Raw | ConvertFrom-Json
if(@($old.before).Count -ne 9){throw 'Expected nine protected files'}
$before=@();$i=0
foreach($entry in $old.before){
 $target=$entry.target;$sha=Hash $target
 $saved=Join-Path $rollback ('{0:D2}-{1}' -f $i,(Split-Path -Leaf $target));$i++
 Copy-Item -LiteralPath $target -Destination $saved
 if((Hash $saved) -ne $sha){throw 'Backup verification failed'}
 $before+=@{target=$target;backup=$saved;sha256=$sha}
}
@{game_dir=$game;exe_sha256=$exeHash;before=$before} | ConvertTo-Json -Depth 6 | Set-Content "$rollback/manifest.json" -Encoding UTF8
Copy-Item -LiteralPath "$previous/Restore.ps1" -Destination "$rollback/Restore.ps1"
foreach($name in 'd3d9.dll','d3d9.pdb','d3d9.map'){Copy-Item -LiteralPath "$candidate/$name" -Destination "$package/candidate/$name"}
# Retain exact integrated source, including the existing uncommitted v11 features.
Compress-Archive -Path "$candidate/src","$candidate/assets","$candidate/build.ps1","$candidate/d3d9.def" -DestinationPath "$package/candidate/source.zip"
$utf8=New-Object System.Text.UTF8Encoding($false,$true)
$planned=@{}
foreach($p in $profiles){
 $text=$utf8.GetString([IO.File]::ReadAllBytes($p))
 if($text -match '(?im)^EngineMaxFrameRate\s*='){
  $text=[regex]::Replace($text,'(?im)^EngineMaxFrameRate\s*=[^\r\n]*','EngineMaxFrameRate=0')
 } else {
  $rx=New-Object regex '(?im)^\[Render\][^\r\n]*(\r?\n)'
  # UTF-8 BOM is retained as a character; allow it before the first section.
  if(!$rx.IsMatch($text)){$rx=New-Object regex '(?im)^\uFEFF\[Render\][^\r\n]*(\r?\n)'}
  if(!$rx.IsMatch($text)){throw "Missing Render section: $p"}
  $text=$rx.Replace($text,('${0}EngineMaxFrameRate=0${1}'),1)
 }
 $planned[$p]=$utf8.GetBytes($text)
}
Assert-Free
foreach($e in $before){if((Hash $e.target) -ne $e.sha256){throw 'Installed file changed during preparation'}}
try {
 Copy-Item -LiteralPath "$package/candidate/d3d9.dll" -Destination "$game/d3d9.dll" -Force
 foreach($p in $profiles){[IO.File]::WriteAllBytes($p,$planned[$p])}
 if((Hash "$game/d3d9.dll") -ne $dllHash){throw 'DLL hash mismatch'}
 foreach($p in $profiles){
  if([D2vrProfileGate]::GetPrivateProfileInt('Render','EngineMaxFrameRate',-1,$p) -ne 0){throw 'Zero setting verification failed'}
 }
 foreach($e in $before){
  $path=[IO.Path]::GetFullPath($e.target)
  if($path -eq [IO.Path]::GetFullPath("$game/d3d9.dll")){continue}
  if($profiles | Where-Object {[IO.Path]::GetFullPath($_) -eq $path}){continue}
  if((Hash $e.target) -ne $e.sha256){throw 'Preserved profile/cache changed'}
 }
 if((Hash "$game/DarknessII.exe") -ne $exeHash){throw 'EXE changed'}
} catch { & "$rollback/Restore.ps1";throw }
$after=@($before | ForEach-Object {@{target=$_.target;sha256=(Hash $_.target)}})
@{dll_sha256=$dllHash;source='v11 source snapshot plus 9fa4b284 limiter';engine_max_frame_rate=0;deployed_utc=[DateTime]::UtcNow.ToString('o');rollback=$rollback;after=$after;game_launched=$false;runtime_validation='NOT TESTED'} | ConvertTo-Json -Depth 5 | Set-Content "$package/deployment.json" -Encoding UTF8
Write-Output "DEPLOYED: $package"
Write-Output "DLL SHA256: $dllHash"
Write-Output 'EngineMaxFrameRate=0 verified in both primary INIs. Nine backups hash-verified; other profiles/caches preserved.'
