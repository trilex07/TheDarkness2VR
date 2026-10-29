$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$source=Get-Content -LiteralPath (Join-Path $repo 'src\stereo_replay.inl') -Raw
$enum=[regex]::Match($source,'(?s)enum RecOp : uint16_t \{.*?\};').Value
$begin=$source.IndexOf('static bool ReplayStream(')
if($begin -lt 0 -or -not $enum){throw 'Replay source boundaries missing'}
$replay=$source.Substring($begin)
$cases=foreach($op in 'SetRenderTarget','SetDepthStencilSurface','Clear','StretchRect','ColorFill','ApplyStateBlock','EndScene') {
    $case=[regex]::Match($replay,"(?s)        case OP_${op}:.*?(?=        case |        default:)")
    if(-not $case.Success){throw "Missing actual case $op"}
    $case.Value
}
$fixture=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'replay_surface_status_test.cpp') -Raw
$generated=Join-Path $env:TEMP 'd2vr-replay-surface-status.cpp'
$fixture.Replace('@STATE_CACHE@',(Get-Content (Join-Path $repo 'src/replay_state_cache.h') -Raw)).Replace('@ENUM@',$enum).Replace('@CASES@',($cases -join "`n"))|Set-Content -LiteralPath $generated -Encoding UTF8
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
$exe=Join-Path $env:TEMP 'd2vr-replay-surface-status.exe'
cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 `"$generated`" /Fo:`"$env:TEMP\d2vr-replay-surface-status.obj`" /Fe:`"$exe`""
if($LASTEXITCODE -ne 0){throw 'Surface receipt build failed'}
$global:LASTEXITCODE=$null
& $exe
if($null -eq $LASTEXITCODE -or $LASTEXITCODE -ne 0){throw 'Surface receipt test failed or did not start'}
