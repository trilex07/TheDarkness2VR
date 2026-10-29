$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
function Extract([string]$path,[string]$signature){
 $text=Get-Content -LiteralPath $path -Raw
 $start=$text.IndexOf($signature,[StringComparison]::Ordinal)
 if($start -lt 0){throw "Missing $signature"}
 $brace=$text.IndexOf('{',$start);$depth=0
 for($i=$brace;$i -lt $text.Length;++$i){
  if($text[$i] -eq '{'){++$depth}
  if($text[$i] -eq '}'){--$depth;if(!$depth){return $text.Substring($start,$i-$start+1)}}
 }
 throw 'Unclosed function'
}
$main=Join-Path $repo 'src/d3d9.cpp'
$body=Extract $main "static void RecordPreSceneClear(IDirect3DDevice9* dev) {"
$body+="`n"+(Extract (Join-Path $repo 'src/stereo_replay.inl') 'static HRESULT STDMETHODCALLTYPE Hook_Clear(')
$mainText=Get-Content -LiteralPath $main -Raw
if($mainText -notmatch 'if\(g_preSceneClearMissed\)d2vr_replay::g_recOverflow=true;'){throw 'Missing fail-closed BeginScene publication gate'}
$generated=Join-Path $env:TEMP 'd2vr-pre-scene-clear.cpp'
(Get-Content (Join-Path $PSScriptRoot 'pre_scene_clear_test.cpp') -Raw).Replace('@IMPLEMENTATION@',$body)|Set-Content -LiteralPath $generated -Encoding utf8
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
$exe=Join-Path $env:TEMP 'd2vr-pre-scene-clear.exe'
cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 `"$generated`" /Fo:`"$env:TEMP\d2vr-pre-scene-clear.obj`" /Fe:`"$exe`""
if($LASTEXITCODE -ne 0){throw 'Build failed'}
& $exe
if($LASTEXITCODE -ne 0){throw 'Clear regression failed'}
