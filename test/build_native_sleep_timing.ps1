$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$output=Join-Path $env:TEMP 'd2vr-native-sleep-timing'
New-Item -ItemType Directory -Force $output|Out-Null
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vsPath 'VC\Auxiliary\Build\vcvarsall.bat'
$sources=@('test/native_sleep_timing_test.cpp','third_party/minhook/src/buffer.c','third_party/minhook/src/hook.c','third_party/minhook/src/trampoline.c','third_party/minhook/src/hde/hde32.c') | ForEach-Object {'"'+(Join-Path $root $_)+'"'}
Push-Location $output
try {
    cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /O2 /std:c++17 $($sources -join ' ') /Fe:native_sleep_timing_test.exe /link /DYNAMICBASE:NO /BASE:0x400000"
    if($LASTEXITCODE -ne 0){throw 'Native sleep observer compile failed'}
    & .\native_sleep_timing_test.exe
    if($LASTEXITCODE -ne 0){throw 'Native sleep observer regression failed'}
}finally{Pop-Location}
