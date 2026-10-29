$ErrorActionPreference='Stop'
$here=Split-Path -Parent $MyInvocation.MyCommand.Path
$repo=Split-Path -Parent (Split-Path -Parent $here)
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vsPath 'VC\Auxiliary\Build\vcvarsall.bat'
$outDir=Join-Path $env:TEMP 'd2vr-small-record-copy-unit'
New-Item -ItemType Directory -Path $outDir -Force | Out-Null
$renderer=Get-Content -LiteralPath (Join-Path $repo 'src\d3d9.cpp') -Raw
if(-not $renderer.Contains('static bool g_smallRecordCopyRequested = false;') -or
   -not $renderer.Contains('static bool g_smallRecordCopyAdmitted = false;') -or
   -not $renderer.Contains('g_smallRecordCopyRequested = GetPrivateProfileIntA("Render", "SmallRecordCopy", 0, chosenIni)!=0;')) {
    throw 'SmallRecordCopy requested/effective admission must remain default OFF'
}
if(-not $renderer.Contains('static std::atomic<bool> g_directRecordCopy{false};') -or
   -not [regex]::IsMatch($renderer,'g_directRecordCopy=false;\s*#ifdef D2VR_MATCHED_CAPTURE\s*g_directRecordCopy = GetPrivateProfileIntA\("Render", "DirectRecordCopy", 0, "\.\\\\Darkness2VR-diagnostic\.ini"\)!=0;\s*#endif')) {
    throw 'DirectRecordCopy must default OFF and accept Render/DirectRecordCopy only in the diagnostic profile/build'
}
$hookDevice=[regex]::Match($renderer,'(?s)void HookDevice\(IDirect3DDevice9\* dev\) \{.*?\r?\n\}')
$admission=$hookDevice.Value.IndexOf('g_smallRecordCopyAdmitted = g_smallRecordCopyAdmission.Admit(g_smallRecordCopyRequested);')
$replayHooks=$hookDevice.Value.IndexOf('d2vr_replay::InstallReplayHooks(dev);')
$firstDeviceHook=$hookDevice.Value.IndexOf('PatchVTable(')
if(-not $hookDevice.Success -or $admission -lt 0 -or $replayHooks -le $admission -or $firstDeviceHook -le $admission) {
    throw 'Guard admission must execute in HookDevice before device/replay hooks are installed'
}
$replay=Get-Content -LiteralPath (Join-Path $repo 'src\stereo_replay.inl') -Raw
$lock=[regex]::Match($replay,'(?s)struct RecLockGuard \{.*?\r?\n\};')
$push=[regex]::Match($replay,'(?s)static inline uint32_t RecPush\(const void\* data, size_t bytes, const d2vr_native_constants::Proof& proof = \{\}\) \{.*?\r?\n\}')
$pushLocked=[regex]::Match($replay,'(?s)static inline uint32_t RecPushLocked\(.*?\r?\n\}')
$emitLocked=[regex]::Match($replay,'(?s)static inline void RecEmitLocked\(.*?\r?\n\}')
$emit=[regex]::Match($replay,'(?s)static inline void RecEmit\(.*?\r?\n\}')
$emitData=[regex]::Match($replay,'(?s)static inline void RecEmitData\(.*?\r?\n\}')
$types=[regex]::Match($replay,'(?s)enum RecOp : uint16_t \{.*?struct Rec \{.*?\r?\n\};')
foreach($part in @($lock,$push,$pushLocked,$emitLocked,$emit,$emitData,$types)){if(-not $part.Success){throw 'Actual recorder source boundary missing'}}
if(-not $pushLocked.Value.Contains('d2vr_record_copy::Copy(g_recData.data() + off, data, bytes, SmallRecordCopyIsEnabled(),') -or
   -not $pushLocked.Value.Contains('g_wideRecordCopy.load()?d2vr_record_copy::kWideLimit:d2vr_record_copy::kSmallLimit')) {
    throw 'Actual RecPush must forward effective guard admission to the existing copy helper'
}
# Exercise production function bodies with the real helper, not a mirrored arena.
$enabled=[regex]::Match($renderer,'(?s)static inline bool SmallRecordCopyIsEnabled\(\) \{.*?\r?\n\}')
if(-not $enabled.Success){throw 'Copy-control integration missing'}
$types.Value|Set-Content -LiteralPath (Join-Path $outDir 'actual_record_types.inl') -Encoding utf8
($enabled.Value + "`n" + $lock.Value + "`n" + $pushLocked.Value + "`n" + $push.Value + "`n" + $emitLocked.Value + "`n" + $emit.Value + "`n" + $emitData.Value) | Set-Content -LiteralPath (Join-Path $outDir 'actual_rec_push.inl') -Encoding utf8
Push-Location $outDir
try {
    $test=Join-Path $here 'small_record_copy_test.cpp'
    cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 /DD2VR_MATCHED_CAPTURE /I`"$outDir`" `"$test`" /Fe:small-record-copy-test.exe"
    if($LASTEXITCODE -ne 0){throw 'small record copy test build failed'}
    & .\small-record-copy-test.exe (Join-Path $repo 'Darkness2VR.ini')
    if($LASTEXITCODE -ne 0){throw 'small record copy tests failed'}
    Write-Host 'PASS default-OFF requested/admitted/config and HookDevice guard probe before device/replay installation'
} finally {Pop-Location}
