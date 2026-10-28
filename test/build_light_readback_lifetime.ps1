param([switch]$Hardware)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$source=Get-Content -LiteralPath (Join-Path $repo 'src\stereo_replay.inl') -Raw
$start=$source.IndexOf('static bool LightReadbackCollecting() {')
$end=$source.IndexOf('// Caller holds g_recLock, including',$start)
if($start -lt 0 -or $end -le $start){throw 'Readback ownership source missing'}
$helpers=$source.Substring($start,$end-$start)
$snapshot=[regex]::Match($source,'(?s)static void LightEndOfWindowSnapshot\(IDirect3DDevice9\* dev\) \{.*?\r?\n\}')
if(-not $snapshot.Success){throw 'Readback expiration source missing'}
$renderer=Get-Content -LiteralPath (Join-Path $repo 'src\d3d9.cpp') -Raw
foreach($name in 'Hook_Reset','Hook_ResetEx') {
    $body=[regex]::Match($renderer,"(?s)HRESULT STDMETHODCALLTYPE $name\(.*?\r?\n\}")
    if(-not $body.Success -or $body.Value.IndexOf('d2vr_replay::ReleaseLightReadbackTargets();') -lt 0) {
        throw "$name must release diagnostic RT ownership"
    }
}
if(-not $source.Contains('CollectLightReadbackTarget(s);')){throw 'RT hook bypasses collection gate'}
$out=Join-Path $env:TEMP 'd2vr-light-readback-lifetime'
New-Item -ItemType Directory -Force -Path $out | Out-Null
Set-Content -LiteralPath (Join-Path $out 'actual_readback.inl') -Value ($helpers+"`n"+$snapshot.Value)
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
$test=Join-Path $PSScriptRoot 'light_readback_lifetime_test.cpp'
Push-Location $out
try {
    cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 /I. `"$test`" /Fe:test.exe"
    if($LASTEXITCODE -ne 0){throw 'Readback lifetime test build failed'}
    & .\test.exe
    if($LASTEXITCODE -ne 0){throw 'Readback lifetime test failed'}
    $gpu=Join-Path $PSScriptRoot 'light_readback_d3d9_test.cpp'
    cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 /I. `"$gpu`" /Fe:d3d9-test.exe /link user32.lib"
    if($LASTEXITCODE -ne 0){throw 'D3D9 readback lifetime build failed'}
    if($Hardware){& .\d3d9-test.exe hardware}else{& .\d3d9-test.exe}
    if($LASTEXITCODE -ne 0){throw 'D3D9 readback lifetime regression failed'}
} finally {Pop-Location}
