$ErrorActionPreference='Stop'
$here=Split-Path -Parent $MyInvocation.MyCommand.Path
$repo=Split-Path -Parent (Split-Path -Parent $here)
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vsPath 'VC\Auxiliary\Build\vcvarsall.bat'
$outDir=Join-Path $env:TEMP 'd2vr-command-history-test'
New-Item -ItemType Directory -Path $outDir -Force | Out-Null
Push-Location $outDir
try {
    $source=Join-Path $here 'command_history_test.cpp'
    cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 `"$source`" /Fe:command-history-test.exe"
    if($LASTEXITCODE -ne 0){throw 'history fixture build failed'}
    & .\command-history-test.exe
    if($LASTEXITCODE -ne 0){throw 'history fixture failed'}
    $renderer=Get-Content -LiteralPath (Join-Path $repo 'src\stereo_replay.inl') -Raw
    $originals=[regex]::Match($renderer,'(?s)struct OrigFns \{.*?\r?\n\};')
    if(-not $originals.Success){throw 'Actual original dispatch declaration missing'}
    $originals.Value | Set-Content -LiteralPath (Join-Path $outDir 'actual_history_orig.inl') -Encoding utf8
    $forward=Join-Path $here 'command_history_forward_test.cpp'
    cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 /I`"$outDir`" `"$forward`" /Fe:command-history-forward-test.exe"
    if($LASTEXITCODE -ne 0){throw 'Actual Windows forwarding test build failed'}
    & .\command-history-forward-test.exe
    if($LASTEXITCODE -ne 0){throw 'Actual Windows forwarding tests failed'}
    python (Join-Path $repo 'tools\perf\test_command_history.py') (Join-Path $outDir 'history-fixture.bin')
    if($LASTEXITCODE -ne 0){throw 'actual header decoder/integration tests failed'}
} finally {Pop-Location}
