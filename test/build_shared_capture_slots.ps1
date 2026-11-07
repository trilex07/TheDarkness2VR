$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$generated=Join-Path $env:TEMP 'd2vr-shared-capture-slots.cpp'
$fixture=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'shared_capture_slots_test.cpp') -Raw
$worker=Get-Content -LiteralPath (Join-Path $repo 'src/stereo_transfer_worker.h') -Raw
$ref=$worker.Substring($worker.IndexOf('template<class T> class Ref'),$worker.IndexOf('struct Job')-$worker.IndexOf('template<class T> class Ref'))
$main=Get-Content -LiteralPath (Join-Path $repo 'src/d3d9.cpp') -Raw
$begin=$main.IndexOf('static bool StartStereoTransfer(')
$start=$main.Substring($begin,$main.IndexOf('static bool FinishStereoTransfer(',$begin)-$begin)
if(!$ref -or !$start){throw 'Missing production transfer boundaries'}
# shared_capture.inl now includes gpu_completion_pause.h (real d3d11_4.h), which
# conflicts with this fixture's fake D3D11 objects. Compile the actual inl with an
# inert CompletionPause (own test: build_gpu_completion_pause.ps1).
$capture=(Get-Content -LiteralPath (Join-Path $repo 'src/shared_capture.inl') -Raw).Replace('#include "gpu_completion_pause.h"','#include "d2vr_fake_completion_pause.h"')
if($capture -notmatch 'd2vr_fake_completion_pause'){throw 'shared_capture.inl completion include changed'}
[IO.File]::WriteAllText((Join-Path $env:TEMP 'd2vr_shared_capture_under_test.inl'),$capture)
[IO.File]::WriteAllText((Join-Path $env:TEMP 'd2vr_fake_completion_pause.h'),"#pragma once`r`nnamespace d2vr_gpu_wait {class CompletionPause {public: template<class D,class C> bool Prepare(D*,C*){return false;} bool Pause(){return false;} bool Drain(){return true;}};}`r`n")
$fixture.Replace('@REPO@',$repo.Replace('\','/')).Replace('@REF@',$ref).Replace('@START_TRANSFER@',$start)|Set-Content -LiteralPath $generated -Encoding utf8
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
$exe=Join-Path $env:TEMP 'd2vr-shared-capture-slots.exe'
cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 /I`"$env:TEMP`" /I`"$repo\src`" `"$generated`" /Fo:`"$env:TEMP\d2vr-shared-capture-slots.obj`" /Fe:`"$exe`""
if($LASTEXITCODE -ne 0){throw 'Shared capture slots build failed'}
& $exe
if($LASTEXITCODE -ne 0){throw 'Shared capture slots regression failed'}
