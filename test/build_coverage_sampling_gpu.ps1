$ErrorActionPreference='Stop'
$here=Split-Path -Parent $MyInvocation.MyCommand.Path
$output=Join-Path $env:TEMP 'd2vr-coverage-sampling'
New-Item -ItemType Directory -Force $output|Out-Null
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vsPath 'VC\Auxiliary\Build\vcvarsall.bat'
Push-Location $output
try {
 $source=Join-Path $here 'coverage_sampling_gpu_test.cpp'
 cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /WX /O2 /std:c++17 `"$source`" /Fe:coverage_sampling_gpu_test.exe /link user32.lib"
 if($LASTEXITCODE -ne 0){throw 'Coverage fixture compilation failed'}
 & .\coverage_sampling_gpu_test.exe
 if($LASTEXITCODE -ne 0){throw 'Coverage fixture failed'}
}finally{Pop-Location}
