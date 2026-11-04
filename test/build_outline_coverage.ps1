param([Parameter(Mandatory=$true)][string]$NativeShader)
$ErrorActionPreference='Stop'
$here=$PSScriptRoot
$shader=(Resolve-Path $NativeShader).Path
$out=Join-Path $here '../test-output/edge-shadow-20261104/gpu'
New-Item -ItemType Directory -Force $out | Out-Null
$vs=& "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
Push-Location $out
try {
 cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 `"$here/outline_coverage_test.cpp`" /Fe:outline-test.exe /link d3dcompiler.lib user32.lib"
 if($LASTEXITCODE){throw 'Compile failed'}
 & ./outline-test.exe $shader
 if($LASTEXITCODE){throw 'Outline GPU regression failed'}
}finally{Pop-Location}
