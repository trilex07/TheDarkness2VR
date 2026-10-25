# Two approved stages on 4fcef42 — results

Branch: codex/diagnostic-engine-present. Exact tested candidate:
`4fcef42e72d0f0f9a7658ab067d0209f86ac4794`; DLL SHA-256
`495A41A7043ED4AADE438554574F6E1972BCC1420644F906EDFF4E9ADA5187C2`.

## Stage 1: PASS for the observed mono prefix

Before launch: no other Steam game process; the previous Singularity process
had exited according to Steam's process log. Existing SteamVR/fpsVR were left
alone. Fresh original EXE SHA/PE/14-site/18-range audit and Authenticode passed.
EXE SHA remained D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1.
DLL and both game INIs backed up; candidate copy hash verified.

PID 29376, start 10:18:25 local, thread 19228. Mono, OpenXR OFF, capture OFF.
Actual swapchain 0F811480. Final saved-log checker result:

```json
{"status":"PASS_OBSERVED_PREFIX","reasons":[],"return_rows":47,"covered_returns":44000,"full_session_proven":false,"gameplay_proven":false,"vr_approved":false}
```

44,000 entries = forwards = returns = ok; statuses=0, errors=0, recursive=0,
fault=0. This covers the prefix through the last logged counters, not unlogged
shutdown calls. The checker never grants approval; stage 2 was separately
authorized by the user, conditional on this PASS. Test process was stopped for
the configuration transition, and final saved log checked before stage 2.

## Stage 2: one request, FAIL / incomplete, no retry

PID 33228, thread 34756. SubmitMode=4, StereoMode=1, OpenXR ON; capture remained
OFF until a loaded room/corridor scene was visually observed. No agent gameplay
movement or parameter sweep. Only one serial (1) was requested. Final sampled
Present counters reached 8,000 entries/forwards/returns/ok, with zero statuses,
errors, recursion or logged exceptions. This is not physical PSVR2 acceptance.

Private partial capture: `%TEMP%/Darkness2VR-matched-33228-1-3822`.
XR frame 3822, closing game-frame/Present serial 3823, epoch/reset generation 1,
device 0F5C9A80, exact caller A94057. Arm and closing entry identities match.
Draw records: 121 per eye, 25 shader hashes per eye, 52 WVP-recognized draws per
eye. VS banks: 991232 bytes = 242 * 4096. These records have no native object
labels or geometry identity sufficient to assign a draw to the right tentacle.

Files written: source-eye0.bmp, manifest.jsonl, vs-banks.bin,
effective-config.json. No source-eye1 or final-eye images. Staged mask=1,
complete=false. Exact rejection:

`no same-eye current-frame inverse projection witness`

The checker rejects the directory because COMPLETE.json is absent. Independently
computed hashes preserve partial evidence, but cannot turn it into a valid pair:

- source-eye0.bmp: `5d3782d4328d99586aa622b4da3c3454bc1efa7ddfafa324ba36a30838ad8550`
- manifest.jsonl: `e32ca04977fdd22ca0739efba500096bc26ca778959ec310e8c0d4601628c023`
- vs-banks.bin: `c6985c1f85f6909da4b76a972258e2153e03ba18817ddbd507f21346e28523e2`
- effective-config.json: `a0a2998e6fc8f413f683be0f1e9cb98807afadce3e9741e10bd1b7ac214ca67d`

The saved source image also contains the pause overlay over the loaded world.
The preceding desktop observation showed loaded gameplay without that overlay;
the exact captured state does not qualify as an unobstructed gameplay frame.
The render-world heuristic is not a pause-state proof. No second request was
made to correct this. The overlay obstructs part of the tentacle evidence.

## Concrete findings for the three visual issues

**P0, right tentacle:** not resolved. Left-side tentacle pixels are visible in
the partial left-eye RT0; right-tentacle visibility cannot be reliably assigned
under the pause overlay. Both eyes have draw records, but no draw has been
proven to be the right-tentacle mesh. No final D3D11 image exists for comparison.
Do not conclude CPU culling or failure to submit the right eye.

**P1, yellow periphery:** a large yellow region is visibly present already in
the right edge of source-eye0.bmp (D3D9 RT0, 1280x855, X8R8G8B8). Therefore a
final-D3D11-only origin is ruled out for this recorded region. Which D3D9 pass
produces it, or whether it is an exposed background from missing geometry,
remains unknown. This does not distinguish frustum, lighting or postprocessing.

**P1, sharpness/resolution:** the actual source RT0 and recorded draw viewports
are 1280x855. Preserved INI requests RenderWidth=3076/RenderHeight=3140. The
actual D3D9 source does not meet that request in this diagnostic run. This is a
concrete source-resolution limitation, not proof of the entire prior PSVR2 blur
cause. Diagnostic builds suppress the historical resolution IAT hook, so do not
extrapolate this run's sizing to an unverified shipping build. No final images
or verified paired projection exist; Luna ray-compare was correctly NOT RUN.
Stored Px/Py values remain candidates, not proven active-camera calibration.

## Next smallest correction (offline first)

The immediate blocker is diagnostic, independently supported by source:
`MatchedProjectionObserved()` has one producer, inside ObserveInverseProjection.
Hook_SetPixelShaderConstantF invokes that producer only when g_replayEye < 0.
Replay's OP_SetPSConstF sends its constants directly through g_orig and never
calls the witness producer. Right-eye Source then requires the absent bit.
The two paths are in src/stereo_replay.inl; guard/bit handling is in
src/matched_capture.inl. Do not remove the per-eye guard or set its bit blindly.

Prepare a diagnostic-only observer for the successful right-eye PS constant
upload, validating the matrix range, shader association, frame/eye identity and
matrix contents. Preserve both recorded native candidate and actually uploaded
matrix with provenance; do not call the live producer if that mutates camera
estimates/rendering. Test missing/partial/stale/wrong-eye uploads and failed API
calls before another proposal. This fixes evidence acquisition, not P0 itself.
The next authorized capture also needs an explicitly unpaused scene, with no
UI action while the one-frame request is pending. No runtime fix was guessed.

## Stop and restoration

Stopped on staged=0, did not increment the serial or restart capture. Preserved
partial files and logs. Escape did not visibly establish a paused/quit state;
the verified test PID was stopped to end the failed trial, without menu-driven
shutdown acceptance. Restored DLL and both game INIs, verified hashes:

- DLL: 9C96DC85F8F0B32C97411D71871A0E2FBBFA27C2818569F986AE78551A72334C
- Game INI: C802439A7AD84E6B4CEA567405B3A5B85BEEA89EB0A00DBF7313FA5437E77436
- Diagnostic INI: 586F536AFEF7BA596F6F2D8789A74B6B37F20501B4902FD3EAF4D33C5E1DA9C0

Original EXE and local INI unchanged. No native FOV, shader, stereo separation,
sharpness or requested resolution change. Private backup/logs/rollback receipt:
`%TEMP%/d2vr-staged-20261025-101824`. Only one capture directory for this PID.
No assets, binaries, logs or large traces committed. No merge.
