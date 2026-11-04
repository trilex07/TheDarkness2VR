$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$output=Join-Path $root 'build/cpu-execution-test'
New-Item -ItemType Directory -Force $output|Out-Null
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vsPath 'VC\Auxiliary\Build\vcvarsall.bat'
$source=Join-Path $root 'test/cpu_execution_test.cpp'
$binary=Join-Path $output "cpu_execution_test-$PID.exe"
Push-Location $output
try {
    cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /O2 /std:c++17 `"$source`" /Fe:`"$binary`""
    if($LASTEXITCODE -ne 0){throw 'CPU execution fixture compile failed'}
    & $binary
    if($LASTEXITCODE -ne 0){throw 'CPU execution fixture failed'}
}finally{Pop-Location}
