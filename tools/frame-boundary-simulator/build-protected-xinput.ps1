$ErrorActionPreference='Stop'
$here=Split-Path -Parent $MyInvocation.MyCommand.Path
$repo=Split-Path -Parent (Split-Path -Parent $here)
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vsPath 'VC\Auxiliary\Build\vcvarsall.bat'
$outDir=Join-Path $env:TEMP 'd2vr-protected-xinput-unit'
New-Item -ItemType Directory -Path $outDir -Force | Out-Null
Push-Location $outDir
try {
    $test=Join-Path $here 'protected_xinput_test.cpp'
    $mh=Join-Path $repo 'third_party\minhook\src'
    cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W3 /O2 /MD /std:c++17 `"$test`" `"$mh\buffer.c`" `"$mh\hook.c`" `"$mh\trampoline.c`" `"$mh\hde\hde32.c`" /Fe:protected-xinput-test.exe"
    if($LASTEXITCODE -ne 0){throw 'protected XInput build failed'}
    & .\protected-xinput-test.exe
    if($LASTEXITCODE -ne 0){throw 'protected XInput regression failed'}
} finally {Pop-Location}
