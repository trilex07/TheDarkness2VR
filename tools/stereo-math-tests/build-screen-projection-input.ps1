$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$out=Join-Path $root 'test-output/screen-projection-input-tests'
New-Item -ItemType Directory -Force $out | Out-Null
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
Push-Location $out
try {
 cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 `"$PSScriptRoot/screen_projection_input_test.cpp`" /Fe:screen-projection-input.exe"
 if($LASTEXITCODE){throw 'Compile failed'}
 & .\screen-projection-input.exe
 if($LASTEXITCODE){throw 'Test failed'}
}finally{Pop-Location}
