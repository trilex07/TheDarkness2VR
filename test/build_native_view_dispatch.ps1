$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$source=Get-Content -LiteralPath (Join-Path $repo 'src\native_camera_cover.inl') -Raw
$begin=$source.IndexOf('static void ObserveNativeViewBoundary(void* view) {')
if($begin -lt 0){throw 'Actual dispatch not found'}
$body=$source.Substring($begin)
$translatedBegin=$source.IndexOf('static void __fastcall ObserveTranslatedFrustum(')
$translatedEnd=$source.IndexOf('static void* g_nativeFrustumGetter', $translatedBegin)
if($translatedBegin -lt 0 -or $translatedEnd -lt 0){throw 'Actual translated-frustum dispatch not found'}
$translated=$source.Substring($translatedBegin,$translatedEnd-$translatedBegin)
$coverBegin=$source.IndexOf('static void __fastcall CoverTranslatedRenderCamera(')
$coverEnd=$source.IndexOf('D2VR_SELECTED_CAMERA_SHIM(TranslatedCameraCoverNear', $coverBegin)
if($coverBegin -lt 0 -or $coverEnd -lt 0){throw 'Actual camera cover dispatch not found'}
$cover=$source.Substring($coverBegin,$coverEnd-$coverBegin)
$fixture=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'native_view_dispatch_fixture.cpp') -Raw
$generated=Join-Path $env:TEMP 'd2vr-native-view-dispatch.cpp'
$fixture.Replace('@IMPLEMENTATION@',$body).Replace('@TRANSLATED@',$translated).Replace('@COVER@',$cover)|Set-Content -LiteralPath $generated -Encoding UTF8
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
$exe=Join-Path $env:TEMP 'd2vr-native-view-phase.exe'
cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 `"$generated`" /Fo:`"$env:TEMP\d2vr-native-view-dispatch.obj`" /Fe:`"$exe`""
if($LASTEXITCODE -ne 0){throw 'Dispatch fixture build failed'}
$global:LASTEXITCODE=$null
& $exe
if($null -eq $LASTEXITCODE -or $LASTEXITCODE -ne 0){throw 'Dispatch fixture failed or did not start'}
