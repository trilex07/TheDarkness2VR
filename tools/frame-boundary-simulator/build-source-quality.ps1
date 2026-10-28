$ErrorActionPreference='Stop'
$here=Split-Path -Parent $MyInvocation.MyCommand.Path
$repo=Split-Path -Parent (Split-Path -Parent $here)
# The native window constructor runs after the D3D factory, before CreateDevice.
# A passing observer unit test alone cannot detect installing it too late.
$renderer=Get-Content -LiteralPath (Join-Path $repo 'src\d3d9.cpp') -Raw
foreach($factory in @(
    @{Name='Direct3DCreate9';Pattern='(?s)IDirect3D9\* WINAPI Direct3DCreate9\(UINT sdk\) \{.*?\r?\n\}';Forward='p_C9Ex(sdk, &ex)'},
    @{Name='Direct3DCreate9Ex';Pattern='(?s)HRESULT WINAPI Direct3DCreate9Ex\(UINT sdk, IDirect3D9Ex\*\* o\) \{.*?\r?\n\}';Forward='p_C9Ex(sdk, &ex)'}
)) {
    $body=[regex]::Match($renderer,$factory.Pattern)
    $install=$body.Value.IndexOf('InstallProtectedSourceQuality();',[StringComparison]::Ordinal)
    $forward=$body.Value.IndexOf($factory.Forward,[StringComparison]::Ordinal)
    if(-not $body.Success -or $install -lt 0 -or $forward -le $install) {
        throw "Source sizing must be installed before forwarding $($factory.Name)"
    }
}
$attach=[regex]::Match($renderer,'(?s)if \(reason == DLL_PROCESS_ATTACH\) \{.*?if \(!LoadReal\(\)\)')
if(-not $attach.Success -or $attach.Value.IndexOf('LoadConfig();') -lt 0) {
    throw 'Source sizing configuration must precede the real factory loader'
}
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vsPath 'VC\Auxiliary\Build\vcvarsall.bat'
$outDir=Join-Path $env:TEMP 'd2vr-source-quality-unit'
New-Item -ItemType Directory -Path $outDir -Force | Out-Null
Push-Location $outDir
try {
    $fixture=Join-Path $here 'source_quality_fixture.cpp'
    $test=Join-Path $here 'source_quality_test.cpp'
    $mh=Join-Path $repo 'third_party\minhook\src'
    cmd /c "`"$vcvars`" x86 && cl /nologo /LD /EHsc /W3 /Od /MD /std:c++17 `"$fixture`" /Fe:fixture.dll /link /OPT:NOICF"
    if($LASTEXITCODE -ne 0){throw 'fixture build failed'}
    cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W3 /O2 /MD /std:c++17 `"$test`" `"$mh\buffer.c`" `"$mh\hook.c`" `"$mh\trampoline.c`" `"$mh\hde\hde32.c`" /Fe:source-quality-test.exe /link user32.lib"
    if($LASTEXITCODE -ne 0){throw 'test build failed'}
    & .\source-quality-test.exe (Join-Path $outDir 'fixture.dll')
    if($LASTEXITCODE -ne 0){throw 'source quality tests failed'}
    & .\source-quality-test.exe (Join-Path $outDir 'fixture.dll') exact
    if($LASTEXITCODE -ne 0){throw 'exact native source tests failed'}
    & .\source-quality-test.exe (Join-Path $outDir 'fixture.dll') exact deferred
    if($LASTEXITCODE -ne 0){throw 'pre-window source sizing lifecycle tests failed'}
} finally {Pop-Location}
