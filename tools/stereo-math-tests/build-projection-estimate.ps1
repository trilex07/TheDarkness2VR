$ErrorActionPreference='Stop'
$here=Split-Path -Parent $MyInvocation.MyCommand.Path
$repo=Split-Path -Parent (Split-Path -Parent $here)
$out=Join-Path $env:TEMP 'd2vr-projection-estimate-test'
New-Item -ItemType Directory -Path $out -Force | Out-Null
$previous=(& git -C $repo show '4665e3f:src/stereo_replay.inl') -join "`n"
if($LASTEXITCODE -ne 0){throw 'Cannot read previous production estimator'}
$begin=$previous.IndexOf('static void PublishProjectionEstimate(')
$end=$previous.IndexOf('static void ObserveInverseProjection(', $begin)
if($begin -lt 0 -or $end -lt 0){throw 'Previous estimator boundaries missing'}
[IO.File]::WriteAllText((Join-Path $out 'previous_projection_estimate.inl'),
    $previous.Substring($begin,$end-$begin).Replace('static void PublishProjectionEstimate','void PublishProjectionEstimate'))
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'Visual Studio C++ tools unavailable'}
$vcvars=Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
Push-Location $out
try {
    cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 /I`"$out`" `"$here\projection_estimate_test.cpp`" /Fe:projection-estimate-test.exe"
    if($LASTEXITCODE -ne 0){throw 'Estimator build failed'}
    & .\projection-estimate-test.exe
    if($LASTEXITCODE -ne 0){throw 'Estimator regression failed'}
} finally {Pop-Location}
