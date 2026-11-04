$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$out=Join-Path $root 'build/smaa-eye-test';New-Item -ItemType Directory -Force $out|Out-Null
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
Push-Location $out
try {
 cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 `"$PSScriptRoot/smaa_eye_gpu_test.cpp`" /Fo:smaa-eye.obj /Fe:smaa-eye.exe /link d3d11.lib dxgi.lib"
 if($LASTEXITCODE){throw 'Compile failed'}
 & .\smaa-eye.exe
 if($LASTEXITCODE){throw 'GPU fixture failed'}
}finally{Pop-Location}
