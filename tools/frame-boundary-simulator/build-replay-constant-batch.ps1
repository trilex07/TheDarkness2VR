$ErrorActionPreference='Stop'
$here=Split-Path -Parent $MyInvocation.MyCommand.Path
$repo=Split-Path -Parent (Split-Path -Parent $here)
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vsPath 'VC\Auxiliary\Build\vcvarsall.bat'
$out=Join-Path $env:TEMP 'd2vr-replay-constant-batch'
New-Item -ItemType Directory -Path $out -Force | Out-Null
$src=Get-Content -LiteralPath (Join-Path $repo 'src\stereo_replay.inl') -Raw
$main=Get-Content -LiteralPath (Join-Path $repo 'src\d3d9.cpp') -Raw
if($main -notmatch 'g_replayConstantBatch = GetPrivateProfileIntA\("Render", "ReplayConstantBatch", 0, chosenIni\)' -or
   $src -notmatch 'batchEnabled = batchEnabled && !MatchedCaptureRecordingActive\(\)' -or
   $src.IndexOf('ApplyShiftToUpload(tmp, src, r.a[0], r.a[1]') -gt $src.IndexOf('constants.Upload(d2vr_replay_batch::Stage::Vertex') -or
   $src.IndexOf('constants.Barrier(') -gt $src.IndexOf('switch ((RecOp)r.op) {')){throw 'Default/capture/barrier/original transform boundaries changed'}
Push-Location $out
try {
    cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 `"$here\replay_constant_batch_test.cpp`" /Fe:replay-constant-batch-test.exe"
    if($LASTEXITCODE -ne 0){throw 'build failed'}
    & .\replay-constant-batch-test.exe
    if($LASTEXITCODE -ne 0){throw 'regression failed'}
} finally {Pop-Location}
