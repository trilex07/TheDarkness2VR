$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$out=Join-Path $root 'test-output/lit-mask-projection-tests'
New-Item -ItemType Directory -Force $out | Out-Null
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
Push-Location $out
try {
 cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 `"$PSScriptRoot/lit_mask_projection_test.cpp`" /Fe:lit-mask-projection.exe"
 if($LASTEXITCODE){throw 'Compile failed'}
 & .\lit-mask-projection.exe
 if($LASTEXITCODE){throw 'Test failed'}
}finally{Pop-Location}
