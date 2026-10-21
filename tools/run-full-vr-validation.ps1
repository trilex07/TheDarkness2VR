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

GateNotRun "runtime_trace_capture" "Requires live game trace (run-darkness2-trace.ps1)"
GateNotRun "d3d9_stereo_harness" "Geometric disparity not validated (placeholder WVP)"
GateNotRun "real_trace_replay" "Run replay-render-trace.py after CSV capture"

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
