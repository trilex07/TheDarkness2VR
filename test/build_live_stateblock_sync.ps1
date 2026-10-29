param([string]$SourceRef)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$out=Join-Path $env:TEMP 'd2vr-live-stateblock-sync'
New-Item -ItemType Directory -Path $out -Force | Out-Null
$source=if($SourceRef){(git -C $repo show "${SourceRef}:src/stereo_replay.inl") -join "`n"}else{Get-Content -LiteralPath (Join-Path $repo 'src/stereo_replay.inl') -Raw}
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
$body=(Extract 'static void SyncActiveShader(IDirect3DDevice9* dev) {')+"`n"+(Extract 'static HRESULT STDMETHODCALLTYPE Hook_SBApply(')
$body=$body.Replace('IDirect3DDevice9','FakeDevice').Replace('IDirect3DStateBlock9','FakeBlock').Replace('IDirect3DVertexShader9','FakeShader').Replace('IDirect3DPixelShader9','FakeShader')
[IO.File]::WriteAllText((Join-Path $out 'actual_stateblock_sync.inl'),$body)
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
Push-Location $out
try {
    cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 /I. `"$PSScriptRoot\live_stateblock_sync_test.cpp`" /Fe:stateblock_sync.exe"
    if($LASTEXITCODE -ne 0){throw 'State-block sync fixture compilation failed'}
    & .\stateblock_sync.exe
    if($LASTEXITCODE -ne 0){throw 'Actual live state-block synchronization failed'}
} finally {Pop-Location}
