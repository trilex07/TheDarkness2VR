param([Parameter(Mandatory=$true)][string]$PrivateShader)
$ErrorActionPreference='Stop'
$here=Split-Path -Parent $MyInvocation.MyCommand.Path
$shader=(Resolve-Path -LiteralPath $PrivateShader).Path
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'C++ tools unavailable'}
$vcvars=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
$out=Join-Path $here '../../test-output/simulator-lab-20261103'
$exe=Join-Path $out 'shadow-clip-test.exe'
$obj=Join-Path $out 'shadow-clip-test.obj'
Push-Location $out
try {
 cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 `"$here/shadow_clip_test.cpp`" /Fo:`"$obj`" /Fe:`"$exe`" /link d3d9.lib user32.lib"
 if($LASTEXITCODE){throw 'compile failed'}
 & $exe $shader
 if($LASTEXITCODE){throw 'validation failed'}
} finally {Pop-Location}
