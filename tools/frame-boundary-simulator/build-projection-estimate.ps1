$ErrorActionPreference='Stop'
$repo=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$source=Get-Content -LiteralPath (Join-Path $repo 'src\stereo_replay.inl') -Raw
$renderer=Get-Content -LiteralPath (Join-Path $repo 'src\d3d9.cpp') -Raw
foreach($required in @('memcpy(&camera[0],&d2vr_replay::g_cameraPx,sizeof(float));',
    'memcpy(&camera[1],&d2vr_replay::g_cameraPy,sizeof(float));',
    'StartRegister, Vector4fCount,camera[0],camera[1],proof);')) {
    if(-not $renderer.Contains($required)){throw "Missing live projection recording: $required"}
}
foreach($required in @('memcpy(&uploadParams.projectionPx,&r.a[2],sizeof(float));',
    'memcpy(&uploadParams.projectionPy,&r.a[3],sizeof(float));',
    'r.a[1], g_activeVs, uploadParams)')) {
    if(-not $source.Contains($required)){throw "Missing replay projection witness: $required"}
}
$start=$source.IndexOf('static float s_estHistX[8]')
$end=$source.IndexOf('static void ObserveInverseProjection', $start)
if($start -lt 0 -or $end -le $start){throw 'Projection estimator source not found'}
$body=$source.Substring($start,$end-$start)
$paramsStart=$source.IndexOf('struct StereoParams {')
$paramsEnd=$source.IndexOf('static bool g_cameraProjectionKnown',$paramsStart)
$params=$source.Substring($paramsStart,$paramsEnd-$paramsStart)
$shiftStart=$source.IndexOf('static void ApplyEyeShiftToMatrix(float* p, int n,')
$shiftEnd=$source.IndexOf('// ------------------------------------------------------------- frame recording',$shiftStart)
$shift=$source.Substring($shiftStart,$shiftEnd-$shiftStart)
$prefix=@'
#include <windows.h>
#include <cassert>
#include <cmath>
#include <limits>
#include <cstdio>
#include <cstring>
static float g_estPx=0,g_estPy=0;
static LONG g_estValid=0;
static bool g_cameraProjectionKnown=false;
static float g_cameraPx=1,g_cameraPy=1;
'@
$test=@'
int main() {
    PublishProjectionEstimate(1,2); PublishProjectionEstimate(1,2);
    assert(!g_estValid);
    PublishProjectionEstimate(1,2); assert(g_estValid && g_estPx==1 && g_estPy==2);
    for(int i=0;i<5;++i) PublishProjectionEstimate(1,2);
    // A new camera must replace ALL startup samples, not repeatedly slot zero.
    for(int cycle=0;cycle<100;++cycle) {
        const float x=cycle%2 ? 1.f:3.f, y=cycle%2 ? 2.f:4.f;
        for(int i=0;i<8;++i) PublishProjectionEstimate(x,y);
        assert(g_estPx==x && g_estPy==y);
        PublishProjectionEstimate(7,15); // isolated plausible matrix outlier
        assert(g_estPx==x && g_estPy==y);
        const auto cursor=s_estHistNext;
        PublishProjectionEstimate(std::numeric_limits<float>::quiet_NaN(),2);
        PublishProjectionEstimate(0,2); PublishProjectionEstimate(1,100);
        assert(s_estHistNext==cursor && g_estPx==x && g_estPy==y);
    }
    const float matrix[16]={2,0,0,0, 0,3,0,0, 0,0,1,1, 0,0,-1,0};
    for(int known=0;known<2;++known) {
        float live[16], replay[16];
        memcpy(live,matrix,sizeof(live)); memcpy(replay,matrix,sizeof(replay));
        g_cameraProjectionKnown=known!=0; g_cameraPx=1.5f; g_cameraPy=2.5f;
        StereoParams sp; sp.transX=.1f; sp.fovScaleX=.5f;
        ApplyEyeShiftToMatrix(live,4,sp,false);
        sp.projectionSnapshot=true;
        sp.projectionPx=known ? g_cameraPx:0;
        sp.projectionPy=known ? g_cameraPy:0;
        // A later inverse upload changes the global camera before right replay.
        g_cameraProjectionKnown=true; g_cameraPx=5; g_cameraPy=6;
        ApplyEyeShiftToMatrix(replay,4,sp,false);
        assert(memcmp(live,replay,sizeof(live))==0);
    }
    puts("PASS: actual transform preserves known AND unknown live projection after later camera upload");
    puts("PASS: actual projection estimator warmup, 100 camera transitions, outliers and rejected values");
}
'@
$out=Join-Path $env:TEMP 'd2vr-projection-estimate'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$geometry=(Join-Path $repo 'src\stereo_geometry.h').Replace('\','/')
Set-Content -LiteralPath (Join-Path $out 'test.cpp') -Value ("#include `"$geometry`"`n"+$prefix+"`n"+$params+"`n"+$body+"`n"+$shift+"`n"+$test)
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vsPath 'VC\Auxiliary\Build\vcvarsall.bat'
Push-Location $out
try {
    cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W3 /O2 /MD /std:c++17 test.cpp /Fe:test.exe"
    if($LASTEXITCODE -ne 0){throw 'Projection estimator build failed'}
    & .\test.exe
    if($LASTEXITCODE -ne 0){throw 'Projection estimator regression failed'}
} finally {Pop-Location}
