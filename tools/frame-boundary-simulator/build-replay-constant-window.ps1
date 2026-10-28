$ErrorActionPreference='Stop'
$here=Split-Path -Parent $MyInvocation.MyCommand.Path
$repo=Split-Path -Parent (Split-Path -Parent $here)
$out=Join-Path $env:TEMP 'd2vr-replay-constant-window'
New-Item -ItemType Directory -Path $out -Force | Out-Null
$src=Get-Content -LiteralPath (Join-Path $repo 'src\stereo_replay.inl') -Raw
$main=Get-Content -LiteralPath (Join-Path $repo 'src\d3d9.cpp') -Raw
if($main -notmatch 'g_replayConstantWindow = GetPrivateProfileIntA\("Render", "ReplayConstantWindow", 0, chosenIni\)' -or
   $src -notmatch 'const bool windowEnabled = batchEnabled && g_replayConstantWindow;' -or
   $src -notmatch 'bindWindowEnabled && IsConstantMaterialBinding\(r.op\)' -or
   $src -notmatch 'constantWindow.Abort\(\); batchEnabled=false;') {throw 'Default/barrier/abort contract changed'}
$begin=$src.IndexOf('    bool batchEnabled = g_replayConstantBatch;',[StringComparison]::Ordinal)
$cases=$src.IndexOf('        switch ((RecOp)r.op) {',$begin,[StringComparison]::Ordinal)
$vs=$src.IndexOf('        case OP_SetVSConstF: {',$cases,[StringComparison]::Ordinal)
$vsEnd=$src.IndexOf('        case OP_SetVSConstI:', $vs,[StringComparison]::Ordinal)
$ps=$src.IndexOf('        case OP_SetPSConstF: {',$vsEnd,[StringComparison]::Ordinal)
$psEnd=$src.IndexOf('        case OP_SetPSConstI:', $ps,[StringComparison]::Ordinal)
$footer=$src.IndexOf('        // Finish the packet at its LAST original constant command,',$psEnd,[StringComparison]::Ordinal)
$end=$src.IndexOf('    g_recLock.store(0,std::memory_order_release);',$footer,[StringComparison]::Ordinal)
if(@($begin,$cases,$vs,$vsEnd,$ps,$psEnd,$footer,$end) | Where-Object {$_ -lt 0}){throw 'Actual replay source boundaries missing'}
# Compile actual admission, forwarding lambda, float cases, look-ahead, SEH and
# receipt aggregation. Non-float commands become observers of the driver banks.
$helperBegin=$src.IndexOf('static bool IsConstantMaterialBinding(unsigned op)',[StringComparison]::Ordinal)
$helperEnd=$src.IndexOf('static void ReplayStream(', $helperBegin,[StringComparison]::Ordinal)
if($helperBegin -lt 0 -or $helperEnd -lt 0){throw 'Actual binding classifier missing'}
[IO.File]::WriteAllText((Join-Path $out 'actual_constant_binding_helpers.inl'),$src.Substring($helperBegin,$helperEnd-$helperBegin))
$enumBegin=$src.IndexOf('enum RecOp : uint16_t {',[StringComparison]::Ordinal)
$enumEnd=$src.IndexOf('};',$enumBegin,[StringComparison]::Ordinal)+2
[IO.File]::WriteAllText((Join-Path $out 'actual_record_ops.inl'),$src.Substring($enumBegin,$enumEnd-$enumBegin))
$actual=$src.Substring($begin,$cases-$begin) + "        switch ((RecOp)r.op) {`n" +
    $src.Substring($vs,$vsEnd-$vs) + $src.Substring($ps,$psEnd-$ps) +
    "        default: if(!IsConstantMaterialBinding(r.op)) CheckBanks(); break;`n        }`n" + $src.Substring($footer,$end-$footer)
[IO.File]::WriteAllText((Join-Path $out 'actual_replay_float_loop.inl'),$actual)
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vsPath 'VC\Auxiliary\Build\vcvarsall.bat'
Push-Location $out
try {
    foreach($flavor in @('production','diagnostic')) {
        $define=if($flavor -eq 'diagnostic'){'/DD2VR_MATCHED_CAPTURE=1'}else{''}
        cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 $define /I`"$out`" `"$here\replay_constant_window_test.cpp`" /Fe:replay-constant-window-test.exe"
        if($LASTEXITCODE -ne 0){throw "$flavor build failed"}
        & .\replay-constant-window-test.exe
        if($LASTEXITCODE -ne 0){throw "$flavor actual replay regression failed"}
    }
} finally {Pop-Location}
