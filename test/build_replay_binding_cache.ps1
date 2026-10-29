$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$source=Get-Content (Join-Path $repo 'src/stereo_replay.inl') -Raw
$config=Get-Content (Join-Path $repo 'src/d3d9.cpp') -Raw
$ini=Get-Content (Join-Path $repo 'Darkness2VR.ini') -Raw
if($config -notmatch 'g_replayBindingFilter = false;' -or $config -notmatch '"ReplayBindingFilter", 0, chosenIni' -or $ini -notmatch 'ReplayBindingFilter=0') {throw 'Binding filter must stay independently OFF by default'}
if($source -notmatch 'bindings.Configure\(g_replayBindingFilter\)') {throw 'Replay must use the configured binding gate'}
$enum=[regex]::Match($source,'(?s)enum RecOp : uint16_t \{.*?\};').Value
$replay=$source.Substring($source.IndexOf('static bool ReplayStream('))
$ops=@('SetTexture','SetVertexDeclaration','SetFVF','SetVertexShader','SetStreamSource','SetStreamSourceFreq','SetIndices','SetPixelShader','DrawPrimitiveUP','DrawIndexedPrimitiveUP','ApplyStateBlock','BufferWrite','SetRenderState')
$cases=foreach($op in $ops){
 $match=[regex]::Match($replay,"(?s)        case OP_${op}:.*?(?=        case |        default:)")
 if(!$match.Success){throw "Missing actual case $op"};$match.Value
}
$out=Join-Path $env:TEMP 'd2vr-replay-binding-cache'
New-Item -ItemType Directory -Force -Path $out|Out-Null
$fixture=Get-Content (Join-Path $PSScriptRoot 'replay_binding_cache_test.cpp') -Raw
[IO.File]::WriteAllText((Join-Path $out 'actual.cpp'),$fixture.Replace('@ENUM@',$enum).Replace('@CASES@',($cases -join "`n")))
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
Push-Location $out
try {
 cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /WX /O2 /MD /std:c++17 /I`"$repo\src`" actual.cpp /Fe:test.exe"
 if($LASTEXITCODE -ne 0){throw 'Binding regression compile failed'}
 & .\test.exe
 if($LASTEXITCODE -ne 0){throw 'Binding regression failed'}
} finally {Pop-Location}
