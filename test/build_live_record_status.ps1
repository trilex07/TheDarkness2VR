$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$source=Get-Content -LiteralPath (Join-Path $repo 'src/stereo_replay.inl') -Raw
function Extract([string]$signature){
 $start=$source.IndexOf($signature,[StringComparison]::Ordinal)
 if($start -lt 0){throw "Missing $signature"}
 $brace=$source.IndexOf('{',$start);$depth=0
 for($i=$brace;$i -lt $source.Length;++$i){
  if($source[$i] -eq '{'){++$depth}
  if($source[$i] -eq '}'){--$depth;if(!$depth){return $source.Substring($start,$i-$start+1)}}
 }
 throw 'Unclosed function'
}
$body=Extract 'static inline HRESULT RecordLiveResult('
foreach($name in 'SetRenderTarget','Clear','SetTransform','SetViewport','SetClipPlane','SetRenderState','SetTextureStageState','SetSamplerState','SetScissorRect','SetVertexDeclaration','SetFVF','SetVSConstI','SetVSConstB','SetStreamSource','SetStreamSourceFreq','SetIndices','SetPSConstI','SetPSConstB','StretchRect','ColorFill'){
 $body+="`n"+(Extract "static HRESULT STDMETHODCALLTYPE Hook_$name(")
}
$enum=[regex]::Match($source,'(?s)enum RecOp : uint16_t \{.*?\};').Value
if(!$enum){throw 'Missing record enum'}
$fixture=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'live_record_status_test.cpp') -Raw
$generated=Join-Path $env:TEMP 'd2vr-live-record-status.cpp'
$fixture.Replace('@ENUM@',$enum).Replace('@IMPLEMENTATION@',$body)|Set-Content -LiteralPath $generated -Encoding utf8
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
$exe=Join-Path $env:TEMP 'd2vr-live-record-status.exe'
cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 `"$generated`" /Fo:`"$env:TEMP\d2vr-live-record-status.obj`" /Fe:`"$exe`""
if($LASTEXITCODE -ne 0){throw 'Live result regression build failed'}
& $exe
if($LASTEXITCODE -ne 0){throw 'Live result regression failed'}
