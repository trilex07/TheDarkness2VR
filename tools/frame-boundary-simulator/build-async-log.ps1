$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$repo = Split-Path -Parent (Split-Path -Parent $here)
$source = Get-Content -LiteralPath (Join-Path $repo 'src\d3d9.cpp') -Raw
$helper = Get-Content -LiteralPath (Join-Path $repo 'src\async_log.h') -Raw
function Body([string]$text, [string]$signature) {
    $start = $text.IndexOf($signature, [StringComparison]::Ordinal)
    if ($start -lt 0) { throw "Missing definition $signature" }
    $brace = $text.IndexOf('{', $start); $depth = 0
    for ($i = $brace; $i -lt $text.Length; ++$i) {
        if ($text[$i] -eq '{') { ++$depth }
        if ($text[$i] -eq '}') { --$depth; if (!$depth) { return $text.Substring($start, $i-$start+1) } }
    }
    throw "Unclosed definition $signature"
}
$main = $source.Substring($source.IndexOf('BOOL APIENTRY DllMain(', [StringComparison]::Ordinal))
if ($main -match 'StartAsyncLogging\(|StopDrain\(|WaitForSingleObject\(|g_emergencyLog.Close\(') { throw 'Worker start/join under loader lock' }
$activeDetach = [regex]::Match($main, '(?s)if \(g_asyncLog.Started\(\)\) \{(.*?)\} else')
if (!$activeDetach.Success -or $activeDetach.Groups[1].Value -match 'Close\(|Wait|EnterCriticalSection') { throw 'Active detach may block or close worker files' }
if ($activeDetach.Groups[1].Value -notmatch 'g_asyncLog.SignalStop\(\)') { throw 'Missing signal-only detach' }
$hook = Body $source 'void HookDevice('
if ($hook.IndexOf('StartAsyncLogging();') -lt 0 -or $hook.IndexOf('StartAsyncLogging();') -gt $hook.IndexOf('InterlockedCompareExchange')) { throw 'Activation must be at HookDevice entry' }
if ($source -notmatch 'GetPrivateProfileIntA\("Render", "AsyncLog", 0, chosenIni\)') { throw 'Async log is not default OFF' }
if ((Get-Content (Join-Path $repo 'Darkness2VR.ini') -Raw) -notmatch '(?m)^AsyncLog=0\r?$') { throw 'INI default changed' }
$start = Body $helper 'bool Start(Sink sink, void* context)'
if ($start.IndexOf('GET_MODULE_HANDLE_EX_FLAG_PIN') -gt $start.IndexOf('CreateThread') -or $start.IndexOf('CreateThread') -gt $start.IndexOf('started_.store(true')) { throw 'Worker published before code pin/create' }
$run = Body $helper 'DWORD Run()'
if ($run.IndexOf('ReleaseSRWLockExclusive') -gt $run.IndexOf('sink_(')) { throw 'Queue lock held during writer I/O' }
$emergency = Body $helper 'class Emergency'
foreach ($signature in @('bool WriteV(', 'bool Record(')) {
    $write = Body $emergency $signature
    if (([regex]::Matches($write, 'handle_\.load\(std::memory_order_acquire\)')).Count -ne 1 -or $write -notmatch 'WriteFile\(handle,') {
        throw 'Emergency writer must use one acquired local handle snapshot'
    }
}
$veh = Body $source 'LONG WINAPI CrashVehHandler('
if ($veh -match '\bLog\(' -or $veh -notmatch 'LogRecord\(\*ep->ExceptionRecord, write\)' -or $veh -notmatch 'return EXCEPTION_CONTINUE_SEARCH;') { throw 'Full VEH routing/disposition regressed' }
if ($veh -notmatch 'budget = &s_crashLogged;' -or $veh -notmatch 'if \(g_asyncLog.Started\(\) && g_emergencyLog.Ready\(\)\)') { throw 'Default combined exception budget changed' }
$parts = @((Body $source 'void Log(const char* fmt, ...)'),
    (Body $source 'static void LogEmergency('),
    (Body $source 'static void LogNativeAdmissionFallback('),
    (Body $source 'static bool AsyncLogSink('),
    (Body $source 'static void StartAsyncLogging()'), $veh)
$out = Join-Path $env:TEMP 'd2vr-async-log-regression'
New-Item -ItemType Directory -Force -Path $out | Out-Null
[IO.File]::WriteAllText((Join-Path $out 'async_log_under_test.inl'), ($parts -join "`r`n"), [Text.Encoding]::UTF8)
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'Visual Studio C++ tools unavailable' }
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
Push-Location $out
try {
    foreach ($test in @('async_log_test', 'persistent_log_test')) {
        $path = Join-Path $here "$test.cpp"
        cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 /I`"$out`" `"$path`" /Fe:$test.exe"
        if ($LASTEXITCODE -ne 0) { throw "$test build failed" }
        & ".\$test.exe"
        if ($LASTEXITCODE -ne 0) { throw "$test failed" }
    }
    & .\async_log_test.exe --admission-failure
    if ($LASTEXITCODE -ne 0) { throw 'actual activation failure regression failed' }
    & .\async_log_test.exe --missing-emergency
    if ($LASTEXITCODE -ne 0) { throw 'actual emergency CreateFile failure regression failed' }
    Write-Host 'PASS: actual producer/activation/VEH bodies; module pin, default OFF, queue lock and loader lifecycle gates'
} finally { Pop-Location }
