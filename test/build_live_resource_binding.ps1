param([string]$SourceRef)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$out=Join-Path $env:TEMP 'd2vr-live-resource-binding'
New-Item -ItemType Directory -Force -Path $out|Out-Null
$source=if($SourceRef){(git -C $repo show "${SourceRef}:src/d3d9.cpp") -join "`n"}else{Get-Content (Join-Path $repo 'src/d3d9.cpp') -Raw}
function Extract([string]$signature) {
    $start=$source.IndexOf($signature,[StringComparison]::Ordinal)
    if($start -lt 0){throw "Missing $signature"}
    $brace=$source.IndexOf('{',$start);$depth=0
    for($i=$brace;$i -lt $source.Length;++$i){
        if($source[$i] -eq '{'){++$depth}
        if($source[$i] -eq '}'){--$depth;if(!$depth){return $source.Substring($start,$i-$start+1)}}
    }
    throw "Unclosed $signature"
}
$body=(Extract 'HRESULT STDMETHODCALLTYPE Hook_SetDepthStencilSurface(')+"`n"+(Extract 'HRESULT STDMETHODCALLTYPE Hook_SetTexture(')
$replay=Get-Content (Join-Path $repo 'src/stereo_replay.inl') -Raw
$replay=$replay.Substring($replay.IndexOf('static bool ReplayStream('))
$cases=foreach($op in 'SetTexture','SetDepthStencilSurface') {
    $part=[regex]::Match($replay,"(?s)        case OP_${op}:.*?(?=        case |        default:)")
    if(!$part.Success){throw "Missing actual replay $op"};$part.Value
}
function FakeTypes([string]$text) {
    $text.Replace('IDirect3DDevice9','Device').Replace('IDirect3DBaseTexture9','Texture').Replace('IDirect3DTexture9','Texture').Replace('IDirect3DSurface9','Surface')
}
[IO.File]::WriteAllText((Join-Path $out 'actual_hooks.inl'),(FakeTypes $body))
[IO.File]::WriteAllText((Join-Path $out 'actual_replay.inl'),(FakeTypes ($cases -join "`n")))
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
Push-Location $out
try {
    cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /WX /O2 /MD /std:c++17 /I. `"$PSScriptRoot\live_resource_binding_test.cpp`" /Fe:binding.exe"
    if($LASTEXITCODE -ne 0){throw 'Live resource binding fixture compilation failed'}
    & .\binding.exe
    if($LASTEXITCODE -ne 0){throw 'Live resource binding regression failed'}
} finally {Pop-Location}
