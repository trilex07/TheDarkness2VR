$ErrorActionPreference='Stop'
$here=Split-Path -Parent $MyInvocation.MyCommand.Path
$output=Join-Path $env:TEMP 'd2vr-stereo-transfer-worker-tests'
New-Item -ItemType Directory -Path $output -Force | Out-Null
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vsPath 'VC\Auxiliary\Build\vcvarsall.bat'
$source=Join-Path $here 'stereo_transfer_worker_test.cpp'
Push-Location $output
try {
    cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /WX /O2 /std:c++17 `"$source`" /Fe:stereo_transfer_worker_test.exe /link user32.lib d3d11.lib dxgi.lib d3dcompiler.lib"
    if($LASTEXITCODE -ne 0) {throw 'compile failed'}
    & .\stereo_transfer_worker_test.exe
    if($LASTEXITCODE -ne 0) {throw 'interop regression failed'}
} finally {Pop-Location}
