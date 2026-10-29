$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$source=Get-Content (Join-Path $repo 'src/stereo_replay.inl') -Raw
$enum=[regex]::Match($source,'(?s)enum RecOp : uint16_t \{.*?\};').Value
$begin=$source.IndexOf('static bool ReplayStream(')
if($begin -lt 0 -or !$enum){throw 'Replay source boundaries missing'}
$replay=$source.Substring($begin)
$ops=@('BeginScene','EndScene','SetTransform','SetViewport','SetClipPlane','SetRenderState','SetTexture',
 'SetTextureStageState','SetSamplerState','SetScissorRect','SetVertexDeclaration','SetFVF','SetVertexShader',
 'SetVSConstI','SetVSConstB','SetStreamSource','SetStreamSourceFreq','SetIndices','SetPixelShader','SetPSConstI','SetPSConstB')
$cases=foreach($op in $ops){
 $match=[regex]::Match($replay,"(?s)        case OP_${op}:.*?(?=        case |        default:)")
 if(!$match.Success){throw "Missing actual case $op"};$match.Value
}
$out=Join-Path $env:TEMP 'd2vr-replay-state-status'
New-Item -ItemType Directory -Path $out -Force | Out-Null
$fixture=Get-Content (Join-Path $PSScriptRoot 'replay_state_status_test.cpp') -Raw
[IO.File]::WriteAllText((Join-Path $out 'actual_test.cpp'),$fixture.Replace('@STATE_CACHE@',(Get-Content (Join-Path $repo 'src/replay_state_cache.h') -Raw)).Replace('@ENUM@',$enum).Replace('@CASES@',($cases -join "`n")))
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
Push-Location $out
try {
 cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 actual_test.cpp /Fe:test.exe"
 if($LASTEXITCODE -ne 0){throw 'Replay state receipt compile failed'}
 & .\test.exe
 if($LASTEXITCODE -ne 0){throw 'Replay state receipt regression failed'}
} finally {Pop-Location}
