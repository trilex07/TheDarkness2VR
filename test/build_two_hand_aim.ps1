$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$out=Join-Path $root 'test-output/two-hand-aim'
New-Item -ItemType Directory -Force $out|Out-Null
$vswhere="${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
$vs=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'MSVC toolchain missing'}
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
$sdk=if($env:OPENXR_SDK){$env:OPENXR_SDK}else{'C:/Users/Trilex/openxr-sdk'}
$source=Join-Path $PSScriptRoot 'two_hand_aim_test.cpp'
Push-Location $out
try {
 cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /WX /O2 /MD /std:c++17 /I`"$sdk/include`" `"$source`" /Fe:two-hand-aim.exe"
 if($LASTEXITCODE){throw 'Compile failed'}
 & ./two-hand-aim.exe
 if($LASTEXITCODE){throw 'Two-hand aim failed'}
}finally{Pop-Location}
