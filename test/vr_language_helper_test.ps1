param([string]$OutputDir)
$ErrorActionPreference='Stop'
if(!$OutputDir){$OutputDir=Join-Path $env:TEMP ('d2vr-language-'+[guid]::NewGuid().ToString('N'))}
$OutputDir=[IO.Path]::GetFullPath($OutputDir)
$game=Join-Path $OutputDir 'game';$root=Join-Path $game 'VRSettings';$cache=Join-Path $game 'Cache.Windows'
New-Item -ItemType Directory -Force $root,$cache | Out-Null
$source=Join-Path $OutputDir 'owner.cpp'
'#include <windows.h>
int main(){Sleep(30000);return 0;}' | Set-Content $source
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
cmd /c "`"$vcvars`" x86 && cl /nologo /O2 `"$source`" /Fe:`"$game/DarknessII.exe`" /Fo:`"$OutputDir/owner.obj`""
if($LASTEXITCODE){throw 'Fixture build failed'}
Copy-Item (Join-Path $PSScriptRoot '../tools/Apply-Languages.ps1') $root
$names=@('H.Misc.cache','H.Misc.toc','H.Misc_en.cache','H.Misc_en.toc','B.Misc_en.cache','B.Misc_en.toc','F.Misc_en.cache','F.Misc_en.toc')
function Hash([string]$p){(Get-FileHash -LiteralPath $p).Hash.ToLowerInvariant()}
$manifest=@{}
foreach($lang in 'en','pl'){
 $folder=Join-Path $root "languages/$lang";New-Item -ItemType Directory -Force $folder | Out-Null
 $manifest[$lang]=@{}
 foreach($n in $names){[IO.File]::WriteAllText((Join-Path $folder $n),"$lang/$n");$manifest[$lang][$n]=Hash (Join-Path $folder $n)}
}
$manifest | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $root 'languages.json')
foreach($n in $names){Copy-Item (Join-Path $root "languages/pl/$n") (Join-Path $cache $n) -Force}
$ini=Join-Path $game 'Darkness2VR.ini'
function Trial([int]$voice,[int]$text,[bool]$corrupt){
 [IO.File]::WriteAllText($ini,"[VRSettings]`r`nPolishVoice=$voice`r`nPolishText=$text`r`n")
 $before=@{};foreach($n in $names){$before[$n]=Hash (Join-Path $cache $n)}
 $owner=Start-Process (Join-Path $game 'DarknessII.exe') -WindowStyle Hidden -PassThru
 $worker=$null
 try {
  $args=@('-NoProfile','-ExecutionPolicy','Bypass','-File',('"'+(Join-Path $root 'Apply-Languages.ps1')+'"'),
   '-OwnerPid',$owner.Id,'-OwnerFileTime',$owner.StartTime.ToUniversalTime().ToFileTimeUtc(),'-IniPath',('"'+$ini+'"'))
  $worker=Start-Process powershell -ArgumentList $args -WindowStyle Hidden -PassThru
  Start-Sleep -Milliseconds 1500
  foreach($n in $names){if((Hash (Join-Path $cache $n)) -ne $before[$n]){throw 'Live game files were changed'}}
  if($corrupt){Add-Content (Join-Path $root 'languages/en/H.Misc.cache') 'tampered'}
  Stop-Process -Id $owner.Id;$owner.WaitForExit()
  if(!$worker.WaitForExit(15000)){throw 'Language helper did not finish'}
  if($worker.ExitCode -ne $(if($corrupt){1}else{0})){throw "Unexpected helper exit $($worker.ExitCode)"}
  foreach($n in $names){
   $lang=if($n -like 'H.Misc.*'){if($text){'pl'}else{'en'}}else{if($voice){'pl'}else{'en'}}
   $expected=if($corrupt){$before[$n]}else{$manifest[$lang][$n]}
   if((Hash (Join-Path $cache $n)) -ne $expected){throw "Incorrect independent profile $voice/$text/$n"}
  }
 } finally {
  if(!$owner.HasExited){Stop-Process -Id $owner.Id}
  if($worker -and !$worker.HasExited){Stop-Process -Id $worker.Id}
 }
}
foreach($voice in 0,1){foreach($text in 0,1){Trial $voice $text $false}}
Trial 0 0 $true
Write-Output 'PASS: four independent language combinations, live-process wait, corrupt profile rejected without mutation'
