$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$output=Join-Path $root 'build/gpu-completion-pause-test'
New-Item -ItemType Directory -Force $output|Out-Null
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vsPath 'VC\Auxiliary\Build\vcvarsall.bat'
$source=Join-Path $root 'test/gpu_completion_pause_test.cpp'
Push-Location $output
try {
    cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /WX /O2 /std:c++17 `"$source`" /Fe:gpu_completion_pause_test.exe /link d3d11.lib"
    if($LASTEXITCODE -ne 0){throw 'GPU completion fixture compile failed'}
    & .\gpu_completion_pause_test.exe
    if($LASTEXITCODE -ne 0){throw 'GPU completion fixture failed'}
}finally{Pop-Location}
