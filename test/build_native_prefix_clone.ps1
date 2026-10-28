$ErrorActionPreference='Stop'
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
$out=Join-Path $env:TEMP 'd2vr-native-prefix'
New-Item -ItemType Directory -Path $out -Force | Out-Null
$source=Join-Path $PSScriptRoot 'native_prefix_clone_test.cpp'
$decoder=Join-Path $PSScriptRoot '../third_party/minhook/src/hde/hde32.c'
Push-Location $out
try {
    cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 `"$source`" `"$decoder`" /Fe:prefix.exe /link /OPT:NOICF"
    if($LASTEXITCODE -ne 0){throw 'Prefix fixture compile failed'}
    & .\prefix.exe
    if($LASTEXITCODE -ne 0){throw 'Prefix fixture failed'}
} finally {Pop-Location}
