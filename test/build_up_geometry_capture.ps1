param([string]$SourceRevision='')
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$out=Join-Path $env:TEMP 'd2vr-up-geometry'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$source=if($SourceRevision){(git show "${SourceRevision}:src/stereo_replay.inl") -join "`n"}else{Get-Content (Join-Path $repo 'src/stereo_replay.inl') -Raw}
function Extract([string]$signature,[bool]$structure=$false) {
    $start=$source.IndexOf($signature,[StringComparison]::Ordinal)
    if($start -lt 0){throw "Missing $signature"}
    $brace=if($structure){$source.IndexOf('{',$start)}else{
        $body=[regex]::Match($source.Substring($start),'\)\s*\{')
        if(!$body.Success){throw "Missing body $signature"};$start+$body.Index+$body.Length-1
    }
    $depth=0;$end=-1
    for($i=$brace;$i -lt $source.Length;++$i){
        if($source[$i] -eq '{'){++$depth}
        if($source[$i] -eq '}'){--$depth;if(!$depth){$end=$i;break}}
    }
    if($end -lt 0){throw "Unclosed $signature"}
    $source.Substring($start,$end-$start+1)+$(if($structure){';'}else{''})
}
$types=[regex]::Match($source,'(?s)enum RecOp : uint16_t \{.*?struct Rec \{.*?\r?\n\};').Value
if(!$types){throw 'Record types missing'}
$core=@((Extract 'struct RecLockGuard {' $true),
    (Extract 'static inline uint32_t RecPushLocked('),(Extract 'static inline uint32_t RecPush('),
    (Extract 'static inline void RecEmitLocked('),(Extract 'static inline void RecEmit(')) -join "`n"
$hooks=@((Extract 'static HRESULT STDMETHODCALLTYPE Hook_DrawPrimitiveUP('),
    (Extract 'static HRESULT STDMETHODCALLTYPE Hook_DrawIndexedPrimitiveUP(')) -join "`n"
$replay=$source.Substring($source.IndexOf('static bool ReplayStream('))
$cases=foreach($name in 'DrawPrimitiveUP','DrawIndexedPrimitiveUP') {
    $part=[regex]::Match($replay,"(?s)        case OP_${name}:.*?(?=        case |        default:)")
    if(!$part.Success){throw "Replay $name missing"};$part.Value
}
$fixture=Get-Content (Join-Path $PSScriptRoot 'up_geometry_capture_test.cpp') -Raw
$fixture=$fixture.Replace('@TYPES@',$types).Replace('@CORE@',$core).Replace('@HOOKS@',$hooks).Replace('@CASES@',($cases -join "`n"))
$generated=Join-Path $out 'up_geometry_capture.cpp'
$fixture.Replace('@REPO@',$repo.Replace('\','/')) | Set-Content $generated -Encoding utf8
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
Push-Location $out
try {
    cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 `"$generated`" /Fe:up_geometry_capture.exe"
    if($LASTEXITCODE -ne 0){throw 'UP geometry build failed'}
    & ./up_geometry_capture.exe
    if($LASTEXITCODE -ne 0){throw 'UP geometry regression failed'}
}finally{Pop-Location}
