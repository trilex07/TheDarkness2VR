param([switch]$RequireConsistentReplay)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$source=Get-Content -LiteralPath (Join-Path $repo 'src\stereo_replay.inl') -Raw
function FunctionText([string]$name) {
    $match=[regex]::Match($source,'(?m)^static[^\r\n]*\b'+$name+'\(')
    if(-not $match.Success){throw "Missing actual function $name"}
    $start=$match.Index
    $brace=$source.IndexOf('{',$start)
    $depth=1;$end=$brace+1
    while($depth -gt 0 -and $end -lt $source.Length) {
        if($source[$end] -eq '{'){$depth++}
        if($source[$end] -eq '}'){$depth--}
        $end++
    }
    if($depth -ne 0){throw "Unbalanced function $name"}
    $source.Substring($start,$end-$start)
}
$lock=[regex]::Match($source,'(?s)static std::atomic<LONG> g_recLock.*?struct RecLockGuard \{.*?\n\};').Value
if(-not $lock){throw 'Recorder lock source missing'}
$enum=[regex]::Match($source,'(?s)enum RecOp : uint16_t \{.*?\};').Value
$record=[regex]::Match($source,'(?s)struct Rec \{.*?\};').Value
$records=foreach($name in 'RecEmitLocked','RecEmit') {FunctionText $name}
$hooks=foreach($name in 'Hook_SetStreamSource','Hook_SetIndices','Hook_DrawPrimitive','Hook_DrawIndexedPrimitive') {FunctionText $name}
$replay=$source.Substring($source.IndexOf('static bool ReplayStream('))
$cases=foreach($op in 'SetStreamSource','SetIndices','DrawPrimitive','DrawIndexedPrimitive','BufferWrite') {
    $case=[regex]::Match($replay,"(?s)        case OP_${op}:.*?(?=        case |        default:)")
    if(-not $case.Success){throw "Missing actual replay case $op"}
    $case.Value
}
$fixture=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'replay_buffer_versions_test.cpp') -Raw
$out=Join-Path $env:TEMP 'd2vr-replay-buffer-versions'
New-Item -ItemType Directory -Force -Path $out|Out-Null
$generated=Join-Path $out 'actual.cpp'
$fixture.Replace('@LOCK@',$lock).Replace('@ENUM@',$enum).Replace('@RECORD@',$record).Replace('@RECORD_FUNCTIONS@',($records -join "`n")).Replace('@HOOK_FUNCTIONS@',($hooks -join "`n")).Replace('@CASES@',($cases -join "`n"))|Set-Content -LiteralPath $generated -Encoding UTF8
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
$exe=Join-Path $out 'test.exe'
cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /WX /O2 /MD /std:c++17 /I`"$repo\src`" `"$generated`" /Fo:`"$out\test.obj`" /Fe:`"$exe`""
if($LASTEXITCODE -ne 0){throw 'Buffer version regression build failed'}
if($RequireConsistentReplay){& $exe --require-consistency}else{& $exe}
if($LASTEXITCODE -ne 0){throw 'Buffer version regression failed'}
