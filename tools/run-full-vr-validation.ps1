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

Gate "Build (d3d9 proxy)" (Join-Path $repo "build.ps1")
Gate "stereo_math_test" (Join-Path $repo "test\build_math_test.ps1")
Gate "stereo_geometry_suite" (Join-Path $root "stereo-math-tests\build.ps1")
Gate "vr_frame_test" (Join-Path $repo "test\build_vr_test.ps1")
Gate "frame_boundary_simulator" (Join-Path $root "frame-boundary-simulator\build.ps1")

Write-Host ""
Write-Host "FULL VR VALIDATION SUMMARY" -ForegroundColor $(if ($failed) { "Red" } else { "Green" })
$results | Format-Table -AutoSize
$pass = ($results | Where-Object Status -eq "PASS").Count
$fail = ($results | Where-Object Status -eq "FAIL").Count
Write-Host "TOTAL SECTIONS: $($results.Count)  PASSED: $pass  FAILED: $fail"
foreach ($name in @("Frame boundary", "Stereo math", "Projection", "OpenXR", "Loading regression", "Mutation")) {
    $st = if ($failed) { "FAIL" } else { "PASS" }
    if ($name -eq "Frame boundary") {
        $fb = $results | Where-Object Section -eq "frame_boundary_simulator"
        $st = if ($fb.Status -eq "PASS") { "PASS" } else { "FAIL" }
    }
    if ($name -eq "Stereo math") {
        $sm = $results | Where-Object Section -eq "stereo_geometry_suite"
        $st = if ($sm.Status -eq "PASS") { "PASS" } else { "FAIL" }
    }
    if ($name -eq "OpenXR") {
        $xr = $results | Where-Object Section -eq "vr_frame_test"
        $st = if ($xr.Status -eq "PASS") { "PASS" } else { "FAIL" }
    }
    if ($name -eq "Loading regression") {
        $fb = $results | Where-Object Section -eq "frame_boundary_simulator"
        $st = if ($fb.Status -eq "PASS") { "PASS (test_rc20_infinite_loading_regression)" } else { "FAIL" }
    }
    if ($name -eq "Mutation") {
        $fb = $results | Where-Object Section -eq "frame_boundary_simulator"
        $st = if ($fb.Status -eq "PASS") { "PASS" } else { "FAIL" }
    }
    if ($name -in @("Projection", "D3D9 harness", "Image analysis")) { $st = "NOT RUN (pending)" }
    Write-Host ("{0}: {1}" -f $name, $st)
}
if ($failed) { exit 1 }
