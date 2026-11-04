$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
function Body($text,$signature) {
 $start=$text.IndexOf($signature);if($start -lt 0){throw "Missing $signature"}
 $brace=$text.IndexOf('{',$start);$depth=0
 for($i=$brace;$i -lt $text.Length;++$i){
  if($text[$i] -eq '{'){++$depth};if($text[$i] -eq '}'){--$depth;if(!$depth){return $text.Substring($start,$i-$start+1)}}
 };throw 'Unclosed body'
}
$main=Get-Content "$root/src/d3d9.cpp" -Raw
$wheel=Get-Content "$root/src/weapon_wheel_input.inl" -Raw
$replay=Get-Content "$root/src/stereo_replay.inl" -Raw
$cover=Get-Content "$root/src/native_camera_cover.inl" -Raw
$bodies=@((Body $main 'static XrVector3f RotVec('),(Body $wheel 'static XrPosef WeaponWheelPose('),
 (Body $replay 'static bool HideMenuFirstPerson('),(Body $cover 'static void NativeCameraUiScale('))
[IO.File]::WriteAllText("$env:TEMP/d2vr-menu-under-test.inl",($bodies -join "`r`n"))
$vswhere="${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
$vs=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
$sdk=if($env:OPENXR_SDK){$env:OPENXR_SDK}else{'C:/Users/Trilex/openxr-sdk'}
Push-Location $root
try {
 New-Item -ItemType Directory -Force build|Out-Null
 cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W3 /O2 /MD /std:c++17 /I`"$sdk/include`" /I`"$env:TEMP`" tools/stereo-math-tests/menu_input_polish_test.cpp /Fo:build/menu-input-polish.obj /Fe:build/menu-input-polish.exe"
 if($LASTEXITCODE -ne 0){throw 'Compile failed'}
 & ./build/menu-input-polish.exe
 if($LASTEXITCODE -ne 0){throw 'Regression failed'}
}finally{Pop-Location}
