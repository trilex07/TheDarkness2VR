param([switch]$RequireConsistentReplay,[switch]$BaselineDeadlocks,[switch]$BaselineCpu)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$source=if($BaselineDeadlocks -or $BaselineCpu){(& git -C $repo show '1dd6205e:src/stereo_replay.inl') -join "`n"}else{Get-Content -LiteralPath (Join-Path $repo 'src\stereo_replay.inl') -Raw}
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
$lock=[regex]::Match($source,'(?s)static (?:auto&|std::atomic<LONG>) g_recLock.*?struct RecLockGuard \{.*?\n\};').Value
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
if($BaselineDeadlocks -or $BaselineCpu){
    # Compile the recovery baseline's actual lock, hooks, buffer interception and
    # replay cases. Only the driver and surrounding frame fixture are doubled.
    (& git -C $repo show '1dd6205e:src/replay_buffers.inl') | Set-Content (Join-Path $out 'baseline_buffers.inl') -Encoding UTF8
    $gateStart=$replay.IndexOf('    LONG expectedLock=0;')
    $gateEnd=$replay.IndexOf('    const uint8_t* dataBase',$gateStart)
    if($gateStart -lt 0 -or $gateEnd -lt 0){throw 'Baseline replay admission missing'}
    $gate=$replay.Substring($gateStart,$gateEnd-$gateStart)
    $shim="`nstatic int g_replayEye=1;`nstatic bool BeginReplayRecording(){`n$gate`nreturn true;}`nstatic bool EndReplayRecording(bool complete){g_bufferWritesApplied=true;g_recLock.store(0);return complete;}`nstatic bool DeferredBufferWritesPending(){return false;}`n"
    $fixture=$fixture.Replace('#include "replay_buffers.inl"',('#include "baseline_buffers.inl"'+$shim)).Replace('CHECK(g_recLock.load()==0);','CHECK(g_recLock.load()==1);').Replace('RecLockGuard guard;return EndReplayRecording(replayComplete);','return EndReplayRecording(replayComplete);')
    $first=$fixture.IndexOf('static void DeferredBenchmark(){');$last=$fixture.IndexOf('int main(',$first)
    $fixture=$fixture.Substring(0,$first)+'static void DeferredBenchmark(){}'+$fixture.Substring($last)
}
$generated=Join-Path $out 'actual.cpp'
$fixture.Replace('@LOCK@',$lock).Replace('@ENUM@',$enum).Replace('@RECORD@',$record).Replace('@RECORD_FUNCTIONS@',($records -join "`n")).Replace('@HOOK_FUNCTIONS@',($hooks -join "`n")).Replace('@CASES@',($cases -join "`n"))|Set-Content -LiteralPath $generated -Encoding UTF8
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
$exe=Join-Path $out 'test.exe'
cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /WX /O2 /MD /std:c++17 /I`"$repo\src`" `"$generated`" /Fo:`"$out\test.obj`" /Fe:`"$exe`""
if($LASTEXITCODE -ne 0){throw 'Buffer version regression build failed'}
if($BaselineCpu){& $exe --cpu-probe;if($LASTEXITCODE -ne 0){throw 'Baseline CPU fixture failed'};exit 0}
if($BaselineDeadlocks){
    foreach($probe in 'draw','worker','lock','unlock','replay'){
        $process=Start-Process -FilePath $exe -ArgumentList @('--sync-probe',$probe) -NoNewWindow -PassThru
        $null=$process.Handle
        if($process.WaitForExit(1500)){throw "Baseline did not reproduce deadlock: $probe, exit $($process.ExitCode)"}
        $process.Kill();$process.WaitForExit()
        Write-Host "PASS reproduced baseline 1dd6205e deadlock: $probe (bounded child timeout)"
    }
    exit 0
}
if($RequireConsistentReplay){& $exe --require-consistency}else{& $exe}
if($LASTEXITCODE -ne 0){throw 'Buffer version regression failed'}

# Each actual-source callback runs in a child so the pre-fix deadlock is bounded.
foreach($probe in 'draw','worker','lock','unlock','replay','draw-write','worker-write','replay-write','upload-write','drain-write','reset') {
    $process=Start-Process -FilePath $exe -ArgumentList @('--sync-probe',$probe) -NoNewWindow -PassThru
    $null=$process.Handle
    if(-not $process.WaitForExit(5000)){$process.Kill();throw "Recorder callback deadlocked: $probe"}
    if($process.ExitCode -ne 0){throw "Recorder callback regression failed: $probe"}
}
