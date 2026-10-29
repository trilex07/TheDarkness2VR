$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$out=Join-Path $env:TEMP 'd2vr-shader-register-bank'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$baseline=(& git -C $repo show '1ff0500:src/shader_register_history.h') -join "`n"
if($LASTEXITCODE -ne 0){throw 'Missing baseline register bank'}
[IO.File]::WriteAllText((Join-Path $out 'baseline_register_bank.h'),
    $baseline.Replace('namespace d2vr_shader_registers {','namespace baseline_registers {'))
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
$source=Join-Path $PSScriptRoot 'shader_register_bank_test.cpp'
Push-Location $out
try {
    cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 /I`"$out`" `"$source`" /Fe:shader-register-bank.exe"
    if($LASTEXITCODE -ne 0){throw 'Register bank build failed'}
    & .\shader-register-bank.exe
    if($LASTEXITCODE -ne 0){throw 'Register bank regression failed'}
} finally {Pop-Location}
