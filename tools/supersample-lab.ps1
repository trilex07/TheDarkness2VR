param([ValidateSet('Install','Restore')][string]$Action,[Parameter(Mandatory=$true)][string]$LabDirectory,
 [ValidateSet('Native','Supersample')][string]$Sampling='Supersample')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$lab=[IO.Path]::GetFullPath($LabDirectory)
$game='C:\Program Files (x86)\Steam\steamapps\common\Darkness II'
if(Get-Process DarknessII -ErrorAction SilentlyContinue){throw 'Close game before lab mutation'}
function Hash([string]$p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash}
function Set-Key([string]$text,[string]$section,[string]$key,[string]$value){
 $pattern='(?ims)^\['+[regex]::Escape($section)+'\]\r?\n.*?(?=^\[|\z)'
 $blocks=[regex]::Matches($text,$pattern)
 if($blocks.Count -ne 1){throw "Expected one $section section"}
 $b=$blocks[0];$kp='(?im)^'+[regex]::Escape($key)+'\s*=.*$'
 if([regex]::Matches($b.Value,$kp).Count -gt 1){throw "Duplicate $key"}
 $replacement=if([regex]::IsMatch($b.Value,$kp)){[regex]::Replace($b.Value,$kp,"$key=$value")}else{$b.Value.TrimEnd()+"`r`n$key=$value`r`n`r`n"}
 return $text.Substring(0,$b.Index)+$replacement+$text.Substring($b.Index+$b.Length)
}
$extra=Join-Path $lab 'supersample-payload.json'
if($Action -eq 'Install'){
 if(Test-Path $extra){throw 'Existing experiment receipt'}
 $target=Join-Path $game 'Darkness2VR.ini'
 if((Hash $target) -ne '68EE5685C7AE9FEC8F1AE2677D551DD90C7BCC21DD66737560815EEF4AB26EA9'){throw 'Unexpected render baseline'}
 # Ordinary lab still tracks original INI as protected. Restore it FIRST before
 # invoking ordinary lab rollback; never rewrite that protected hash to bless a change.
 & (Join-Path $PSScriptRoot 'simulator-lab.ps1') -Action Install -LabDirectory $lab -ExpectedInstalledDll '4A0320B2C9369EA8696C65786B4A871434D855CB62540859209DC95A4F559EC7'
 $diagnostic=Join-Path $lab 'candidate-diagnostic.ini'
 $dt=[IO.File]::ReadAllText($diagnostic)
 foreach($kv in @(@('CPUSyncTiming','1'),@('HookCostSampling','1'),@('SpecularFootprintExperiment','1'),@('SkinMipFilterExperiment','1'))){$dt=Set-Key $dt 'Diagnostics' $kv[0] $kv[1]}
 [IO.File]::WriteAllText($diagnostic,$dt,[Text.Encoding]::ASCII)
 & (Join-Path $PSScriptRoot 'simulator-lab.ps1') -Action Update -LabDirectory $lab
 $runtime=Join-Path $lab 'runtime';New-Item -ItemType Directory -Force $runtime|Out-Null
 @{view_mode='both';layout='side_by_side';headset_profile='psvr2';render_width=2676;render_height=2728;asymmetric_fov=$true;fov_degrees=90;ipd_mm=64;zoom_mode='fill';zoom_scale=1;window_width=1728;window_height=924;full_render=$false;show_stats=$false;gpu_timing=$true;move_speed=3;move_boost=4;input_polling='app';preview_fps=90}|ConvertTo-Json|Set-Content (Join-Path $runtime 'settings.json')
 $backup=Join-Path $lab 'rollback/Darkness2VR.ini';Copy-Item -LiteralPath $target -Destination $backup
 $text=[IO.File]::ReadAllText($target)
 $width=if($Sampling -eq 'Native'){'2676'}else{'3345'}
 $height=if($Sampling -eq 'Native'){'2728'}else{'3410'}
 foreach($kv in @(@('RenderWidth',$width),@('RenderHeight',$height),@('SourceQuality','5'),@('MatchEyeResolutionToSource','0'))){$text=Set-Key $text 'Render' $kv[0] $kv[1]}
 $candidate=Join-Path $lab 'candidate-render.ini';[IO.File]::WriteAllText($candidate,$text,[Text.Encoding]::ASCII)
 $saves=@();$saveRoot=Join-Path $env:APPDATA 'DarknessII'
 foreach($relative in 'EE.cfg','Editor.cfg','76561198334985719/settings','76561198334985719/CONTINUE.SAV'){
  $path=Join-Path $saveRoot $relative;$copy=Join-Path $lab ('save-backup/'+$relative)
  New-Item -ItemType Directory -Force (Split-Path $copy)|Out-Null;Copy-Item -LiteralPath $path -Destination $copy
  if((Hash $path) -ne (Hash $copy)){throw 'Save backup mismatch'}
  $saves+=@{target=$path;backup=$copy;hash=(Hash $path)}
 }
 $r=@{target=$target;backup=$backup;before=(Hash $target);after=(Hash $candidate);saves=$saves;sampling=$Sampling;status='prepared'}
 $r|ConvertTo-Json -Depth 6|Set-Content $extra
 if((Hash $backup) -ne $r.before){throw 'INI backup mismatch'}
 Copy-Item -LiteralPath $candidate -Destination $target -Force
 if((Hash $target) -ne $r.after){throw 'INI deployment mismatch'}
 Write-Output "Sampling lab installed: $Sampling source request${width}x${height}, XR recommended2676x2728 unchanged. Use this wrapper Restore."
}else{
 $r=Get-Content $extra -Raw|ConvertFrom-Json
 if((Hash $r.backup) -ne $r.before -or ((Hash $r.target) -ne $r.after -and (Hash $r.target) -ne $r.before)){throw 'Render identity changed'}
 foreach($f in $r.saves){if((Hash $f.backup) -ne $f.hash){throw 'Save backup changed'}}
 Copy-Item -LiteralPath $r.backup -Destination $r.target -Force
 if((Hash $r.target) -ne $r.before){throw 'Render rollback failed'}
 & (Join-Path $PSScriptRoot 'simulator-lab.ps1') -Action Restore -LabDirectory $lab
 foreach($f in $r.saves){Copy-Item -LiteralPath $f.backup -Destination $f.target -Force;if((Hash $f.target) -ne $f.hash){throw 'Save rollback failed'}}
 Write-Output 'RESTORED user DLL/diagnostic/render INI and all four saves/settings.'
}
