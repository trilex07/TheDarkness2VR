param([ValidateSet('Install','Update','Restore')][string]$Action,
 [string]$GameDir='C:\Program Files (x86)\Steam\steamapps\common\Darkness II')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$lab=Join-Path $root 'test-output/simulator-lab-20261103'
$receipt=Join-Path $lab 'payload.json'
if(Get-Process DarknessII -ErrorAction SilentlyContinue){throw 'Close game before changing lab payload'}
function Hash([string]$p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash}
if($Action -eq 'Install'){
 if(Test-Path $receipt){throw 'Existing lab receipt; restore before a new install'}
 $manifest=Join-Path $root 'tools-local/OpenXR-Simulator/bin-x86/openxr_simulator.json'
 if(!(Test-Path $manifest)){throw 'Build x86 simulator first'}
 $before=Hash (Join-Path $GameDir 'd3d9.dll')
 if($before -ne 'E56BF027875A6FF3997E0B6A51A0DC43F52DD86C2EB39AE79D4C5EAB073FA5CA'){throw 'Unexpected installed baseline'}
 & (Join-Path $PSScriptRoot 'assert-candidate-profile.ps1') -CandidateDll (Join-Path $root 'd3d9.dll') -RenderIni @((Join-Path $GameDir 'Darkness2VR.ini')) -DiagnosticIni (Join-Path $GameDir 'Darkness2VR-diagnostic.ini') | Out-Host
 New-Item -ItemType Directory -Force (Join-Path $lab 'rollback') | Out-Null
 $files=@(); foreach($name in 'd3d9.dll','Darkness2VR-diagnostic.ini'){
  $target=Join-Path $GameDir $name; $backup=Join-Path $lab "rollback/$name"
  Copy-Item -LiteralPath $target -Destination $backup
  if((Hash $target) -ne (Hash $backup)){throw 'Backup mismatch'}
  $files+=@{target=$target;backup=$backup;before=(Hash $target)}
 }
 $protected=@(); foreach($name in 'DarknessII.exe','Darkness2VR.ini','openxr_loader.dll'){
  $target=Join-Path $GameDir $name; $protected+=@{target=$target;hash=(Hash $target)}
 }
 $candidate=Join-Path $lab 'candidate.dll'; Copy-Item (Join-Path $root 'd3d9.dll') $candidate -Force
 $config=[IO.File]::ReadAllText((Join-Path $GameDir 'Darkness2VR-diagnostic.ini'))
 $config+="`r`n[Simulator]`r`nEnabled=1`r`nRuntimeJson=$manifest`r`nDataDirectory=$(Join-Path $lab 'runtime')`r`n"
 $configPath=Join-Path $lab 'candidate-diagnostic.ini';[IO.File]::WriteAllText($configPath,$config,[Text.Encoding]::ASCII)
 $files[0].after=Hash $candidate;$files[1].after=Hash $configPath
 @{files=$files;protected=$protected;source=(git -C $root rev-parse HEAD);status='prepared'} | ConvertTo-Json -Depth 5 | Set-Content $receipt
 Copy-Item $candidate (Join-Path $GameDir 'd3d9.dll') -Force
 Copy-Item $configPath (Join-Path $GameDir 'Darkness2VR-diagnostic.ini') -Force
 foreach($f in $files){if((Hash $f.target) -ne $f.after){throw 'Installation verification failed'}}
 foreach($f in $protected){if((Hash $f.target) -ne $f.hash){throw 'Protected file changed'}}
 Write-Output 'Lab installed; runtime override process-only; exact baseline rollback verified.'
}elseif($Action -eq 'Update'){
 $r=Get-Content $receipt -Raw | ConvertFrom-Json
 foreach($f in $r.files){
  if((Hash $f.backup) -ne $f.before -or (Hash $f.target) -ne $f.after){throw 'Lab payload changed; refusing update'}
 }
 foreach($f in $r.protected){if((Hash $f.target) -ne $f.hash){throw 'Protected file changed'}}
 $configPath=Join-Path $lab 'candidate-diagnostic.ini'
 if(!(Test-Path $configPath)){throw 'Candidate config missing'}
 Copy-Item $receipt (Join-Path $lab ('payload-before-update-'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmss')+'.json'))
 Copy-Item (Join-Path $root 'd3d9.dll') (Join-Path $lab 'candidate.dll') -Force
 $r.files[0].after=Hash (Join-Path $lab 'candidate.dll');$r.files[1].after=Hash $configPath
 $r.source=git -C $root rev-parse HEAD
 $r | ConvertTo-Json -Depth 5 | Set-Content $receipt
 Copy-Item (Join-Path $lab 'candidate.dll') $r.files[0].target -Force
 Copy-Item $configPath $r.files[1].target -Force
 foreach($f in $r.files){if((Hash $f.target) -ne $f.after){throw 'Update verification failed'}}
 Write-Output 'Lab updated; exact original rollback retained.'
}else{
 $r=Get-Content $receipt -Raw | ConvertFrom-Json
 foreach($f in $r.files){
  if((Hash $f.backup) -ne $f.before){throw 'Backup identity mismatch'}
  $h=Hash $f.target;if($h -ne $f.after -and $h -ne $f.before){throw 'Target changed outside lab; refusing overwrite'}
 }
 foreach($f in $r.protected){if((Hash $f.target) -ne $f.hash){throw 'Protected file changed'}}
 foreach($f in $r.files){Copy-Item -LiteralPath $f.backup -Destination $f.target -Force;if((Hash $f.target) -ne $f.before){throw 'Rollback mismatch'}}
 Write-Output 'RESTORED exact baseline DLL and diagnostic INI; protected files match.'
}
