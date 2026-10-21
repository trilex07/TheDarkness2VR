# Full offline VR validation gate — required before any headset candidate.
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$repo = Split-Path -Parent $root
$results = @()
$failed = $false

function Gate($section, $script) {
    Write-Host "=== $section ===" -ForegroundColor Cyan
    try {
        & $script
        if ($LASTEXITCODE -ne 0) { throw "exit $LASTEXITCODE" }
        $script:results += [pscustomobject]@{ Section = $section; Status = "PASS" }
    } catch {
        $script:results += [pscustomobject]@{ Section = $section; Status = "FAIL" }
        $script:failed = $true
        Write-Host "FAIL: $section - $_" -ForegroundColor Red
    }
}

function GateNotRun($section, $reason) {
    $script:results += [pscustomobject]@{ Section = $section; Status = "NOT RUN"; Detail = $reason }
}

Gate "Build (d3d9 proxy)" (Join-Path $repo "build.ps1")
Gate "stereo_math_test" (Join-Path $repo "test\build_math_test.ps1")
Gate "stereo_geometry_suite" (Join-Path $root "stereo-math-tests\build.ps1")
Gate "vr_frame_test" (Join-Path $repo "test\build_vr_test.ps1")
Gate "frame_boundary_simulator" (Join-Path $root "frame-boundary-simulator\build.ps1")

foreach ($g in @(
    @{ Section = 'vanilla_launch'; Detail = 'run-lifetime-experiment.ps1 -Mode A' }
    @{ Section = 'mod_no_hook'; Detail = 'run-lifetime-experiment.ps1 -Mode B' }
    @{ Section = 'presentmon_etw'; Detail = 'run-presentmon-capture.ps1' }
    @{ Section = 'overlay_matrix'; Detail = 'run-steam-overlay-matrix.ps1' }
    @{ Section = 'crash_dump_analysis'; Detail = 'WinDbg on WER minidump when available' }
    @{ Section = 'hook_recursion_test'; Detail = 'PresentHook maxTlsDepth + harness' }
    @{ Section = 'minimal_passthrough_present'; Detail = 'PresentHookLevel=0 shadow-vtable' }
    @{ Section = '1000_consecutive_presents'; Detail = 'BLOCKED until hook hits ETW swapchain' }
    @{ Section = 'runtime_trace_capture'; Detail = 'run-darkness2-trace.ps1' }
    @{ Section = 'd3d9_stereo_harness'; Detail = 'Geometric disparity not validated' }
    @{ Section = 'real_trace_replay'; Detail = 'replay-render-trace.py' }
    @{ Section = 'repeated_startup_tests'; Detail = '20x cold launch — not started' }
)) { GateNotRun $g.Section $g.Detail }

Write-Host ""
$mandatoryNotRun = @("runtime_trace_capture", "d3d9_stereo_harness")
$blocked = ($results | Where-Object { $_.Status -eq "NOT RUN" -and $_.Section -in $mandatoryNotRun }).Count
Write-Host "FULL VR VALIDATION SUMMARY" -ForegroundColor $(if ($failed -or $blocked) { "Red" } else { "Green" })
$results | Format-Table Section, Status, Detail -AutoSize
$pass = ($results | Where-Object Status -eq "PASS").Count
$fail = ($results | Where-Object Status -eq "FAIL").Count
$notRun = ($results | Where-Object Status -eq "NOT RUN").Count
Write-Host "TOTAL: $($results.Count)  PASS: $pass  FAIL: $fail  NOT RUN: $notRun"
if ($failed) { exit 1 }
if ($blocked) {
    Write-Host "BLOCKED: mandatory runtime gates not executed." -ForegroundColor Red
    exit 2
}
