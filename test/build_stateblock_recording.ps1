$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$source=Get-Content (Join-Path $repo 'src/stereo_replay.inl') -Raw
function Extract([string]$name){
    $start=$source.IndexOf("static HRESULT STDMETHODCALLTYPE Hook_${name}(",[StringComparison]::Ordinal)
    if($start -lt 0){throw "Missing $name"}
    $brace=$source.IndexOf('{',$start);$depth=0
    for($i=$brace;$i -lt $source.Length;++$i){
        if($source[$i] -eq '{'){++$depth}
        if($source[$i] -eq '}'){--$depth;if(!$depth){return $source.Substring($start,$i-$start+1)}}
    }
    throw "Unclosed $name"
}
if($source -notmatch '60,.*Hook_BeginStateBlock' -or $source -notmatch '61,.*Hook_EndStateBlock'){throw 'Begin/End device hooks must both be installed'}
$out=Join-Path $env:TEMP 'd2vr-stateblock-recording'
New-Item -ItemType Directory -Force -Path $out|Out-Null
[IO.File]::WriteAllText((Join-Path $out 'actual_stateblock_recording.inl'),(Extract 'BeginStateBlock')+"`n"+(Extract 'EndStateBlock'))
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
Push-Location $out
try {
    cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /WX /O2 /MD /std:c++17 /I. `"$PSScriptRoot\stateblock_recording_test.cpp`" /Fe:test.exe"
    if($LASTEXITCODE -ne 0){throw 'Stateblock recording fixture build failed'}
    & .\test.exe
    if($LASTEXITCODE -ne 0){throw 'Stateblock recording regression failed'}
} finally {Pop-Location}
