$ErrorActionPreference='Stop'
$here=$PSScriptRoot
$output=Join-Path $env:TEMP ('d2vr-material-constants-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $output|Out-Null
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vsPath 'VC\Auxiliary\Build\vcvarsall.bat'
$source=Join-Path $here 'render_cpu_optimization_test.cpp'
Push-Location $output
try {
    cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /WX /O2 /MD /std:c++17 `"$source`" /Fe:render_cpu_optimization_test.exe /link user32.lib"
    if($LASTEXITCODE -ne 0){throw 'compile failed'}
    & .\render_cpu_optimization_test.exe
    if($LASTEXITCODE -ne 0){throw 'material constants driver regression failed'}
} finally {Pop-Location}
