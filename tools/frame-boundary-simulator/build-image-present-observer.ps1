$ErrorActionPreference='Stop'
$here=Split-Path -Parent $MyInvocation.MyCommand.Path
$repo=Split-Path -Parent (Split-Path -Parent $here)
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vsPath 'VC\Auxiliary\Build\vcvarsall.bat'
$outDir=Join-Path $env:TEMP 'd2vr-image-present-unit'
New-Item -ItemType Directory -Path $outDir -Force | Out-Null
Push-Location $outDir
try {
    $fixture=Join-Path $here 'image_present_fixture.cpp'
    $test=Join-Path $here 'image_present_observer_test.cpp'
    $mh=Join-Path $repo 'third_party\minhook\src'
    cmd /c "`"$vcvars`" x86 && cl /nologo /LD /EHsc /W3 /Od /MD /std:c++17 `"$fixture`" /Fe:fixture.dll /link /OPT:NOICF"
    if($LASTEXITCODE -ne 0){throw 'fixture build failed'}
    cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W3 /O2 /MD /std:c++17 `"$test`" `"$mh\buffer.c`" `"$mh\hook.c`" `"$mh\trampoline.c`" `"$mh\hde\hde32.c`" /Fe:observer-test.exe"
    if($LASTEXITCODE -ne 0){throw 'observer build failed'}
    & .\observer-test.exe (Join-Path $outDir 'fixture.dll')
    if($LASTEXITCODE -ne 0){throw 'observer tests failed'}
    & .\observer-test.exe (Join-Path $outDir 'fixture.dll') timing-disabled
    if($LASTEXITCODE -ne 0){throw 'disabled forwarding timing failed'}
    & .\observer-test.exe (Join-Path $outDir 'fixture.dll') unqualified-create
    if($LASTEXITCODE -ne 0){throw 'bounded acquisition/admission receipts failed'}
    & .\observer-test.exe (Join-Path $outDir 'fixture.dll') deny
    if($LASTEXITCODE -ne 0){throw 'EXE write rejection failed'}
    & .\observer-test.exe (Join-Path $outDir 'fixture.dll') hr-error
    if($LASTEXITCODE -ne 0){throw 'Present error admission failed'}
    & .\observer-test.exe (Join-Path $outDir 'fixture.dll') thread-change
    if($LASTEXITCODE -ne 0){throw 'Present thread admission failed'}
} finally {Pop-Location}
