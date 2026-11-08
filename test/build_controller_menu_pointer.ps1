$ErrorActionPreference='Stop'
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
$sdk=if($env:OPENXR_SDK){$env:OPENXR_SDK}else{'C:/Users/Trilex/openxr-sdk'}
$source=Join-Path $PSScriptRoot 'controller_menu_pointer_test.cpp'
$wheel=Get-Content (Join-Path $PSScriptRoot '../src/weapon_wheel_input.inl') -Raw
$start=$wheel.IndexOf('static void ApplyMenuTriggerConfirm(XINPUT_GAMEPAD& gp) {')
if($start -lt 0){throw 'Missing actual menu confirm'}
$brace=$wheel.IndexOf('{',$start);$depth=0;$end=-1
for($i=$brace;$i -lt $wheel.Length;$i++){
 if($wheel[$i] -eq '{'){++$depth}
 if($wheel[$i] -eq '}'){--$depth;if(!$depth){$end=$i+1;break}}
}
if($end -le $start){throw 'Unclosed actual menu confirm'}
[IO.File]::WriteAllText((Join-Path $env:TEMP 'd2vr_menu_confirm_under_test.inl'),$wheel.Substring($start,$end-$start))
$exe=Join-Path $env:TEMP 'd2vr-controller-menu-pointer.exe'
cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /WX /O2 /std:c++17 /I`"$sdk/include`" /I`"$env:TEMP`" `"$source`" /Fo:`"$env:TEMP\d2vr-controller-menu-pointer.obj`" /Fe:`"$exe`""
if($LASTEXITCODE -ne 0){throw 'Controller menu pointer compile failed'}
& $exe
if($LASTEXITCODE -ne 0){throw 'Controller menu pointer regression failed'}
