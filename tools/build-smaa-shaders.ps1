$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$fxc=Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin\*\x86\fxc.exe"|Sort-Object FullName|Select-Object -Last 1 -ExpandProperty FullName
if(!$fxc){throw 'Windows SDK fxc missing'}
foreach($entry in 'EdgeVS','WeightVS','BlendVS','EdgePS','WeightPS','BlendPS') {
 $profile=if($entry.EndsWith('VS')){'vs_4_0'}else{'ps_4_0'}
 & $fxc /nologo /O3 /T $profile /E $entry /Vn "kSmaa$entry" /Fh (Join-Path $root "src/smaa_$entry.h") (Join-Path $root 'src/smaa_eye.hlsl')
 if($LASTEXITCODE){throw "SMAA $entry compile failed"}
}
