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
$fixture.Replace('@REPO@',$repo.Replace('\','/')).Replace('@REF@',$ref).Replace('@START_TRANSFER@',$start)|Set-Content -LiteralPath $generated -Encoding utf8
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
$exe=Join-Path $env:TEMP 'd2vr-shared-capture-slots.exe'
cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 `"$generated`" /Fo:`"$env:TEMP\d2vr-shared-capture-slots.obj`" /Fe:`"$exe`""
if($LASTEXITCODE -ne 0){throw 'Shared capture slots build failed'}
& $exe
if($LASTEXITCODE -ne 0){throw 'Shared capture slots regression failed'}
