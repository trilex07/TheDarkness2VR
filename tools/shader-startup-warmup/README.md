# Startup shader warmup prototype

Experimental, default OFF. `Render/StartupShaderWarmup=1` in the selected mod INI
runs once on the first successful D3D9 device creation, BEFORE proxy shader hooks
and before returning that device to the engine. No Present/Reset callback, native
owner access, shader binding, image patch, persistent COM cache or worker thread.

Input: `%LOCALAPPDATA%/Darkness2VR/startup-shaders.bin`. No input is installed by
this branch. Missing/invalid input leaves device creation unchanged. Entire package
is structurally admitted before calls: SM3 only, <=256 shaders, <=32768 bytes each,
<=4 MiB total. Each successful CreateShader is immediately released. First failure
stops work. The 10-second budget is checked BETWEEN driver calls, not a timeout
that can interrupt a stuck call. Startup file reading also has no hard timeout.

Purpose: test whether the driver reuses compilation from early same-device
creation. Native getters and their failure/bytecode ownership remain untouched.
This is a NEW driver-cache experiment, not the rejected early native getter call.
It does not guarantee the driver retains compiled code or avoids first-draw work.
Original and mod-transformed shader variants can differ; warmed original blobs
may not warm the active rewritten PS. No automatic enablement or90Hz claim.

Private packaging, no redistribution:

```powershell
python tools/shader-startup-warmup/pack.py --trace <existing-trace.txt> --shader-dir <private-shader-directory> --output <new-private-path.bin>
# Or supply explicit verified SM3 .bin files instead of --trace/--shader-dir.
python -m unittest discover -s tools/shader-startup-warmup -p test_pack.py -v
./tools/shader-startup-warmup/build-test.ps1 -Package <private-package.bin>
./tools/shader-startup-warmup/build-driver-probe.ps1
```

Packer deduplicates exact byte strings. Trace FNV selects candidate files, not an
identity certificate. Verify the JSON SHA256 provenance. Never replace existing
output silently. Local run43052 corpus yielded19 unique shaders/18592 bytes;
84 of103 trace hashes remain unmatched. This is insufficient coverage to claim
the measured97.865 ms shader hitch is fixed.

`build-driver-probe.ps1 -Run -Package <file>` explicitly executes a short hidden
D3D9Ex fixture; default invocation only builds. Run only in a free GPU session,
never during gaming/headset use. It loads system D3D9, not the proxy; creates a
64x64 hidden device and runs three create/release passes. First pass is not a cold
cache proof. Does not render, launch Steam, change shader-cache settings or modify
installed files. Timing includes Release; results are not gameplay FPS.

MAIN integration gate: review/cherry-pick the implementation commit, preserve the
baseline, stage the private package only during a separately authorized test and
compare OFF/ON using the SAME binary/profile and scene. Inspect warmup receipt,
actual shader coverage, first-use shader waits and startup overhead. GPU fixture
reuse alone is not proof of game benefit. If no material shader-wait reduction,
leave OFF and abandon this experiment; do not extend it into unsafe owner caching.
Rollback: StartupShaderWarmup=0 and restart; package may remain unused. SIDE has
not changed any installed INI/DLL, invoked Steam or run a game.

Offline verification: x86 production and diagnostic DLL builds PASS;43 C++ core
checks including the real private package with FAKE GPU ops PASS;5 pack/integration
checks PASS; existing stereo transfer5 checks PASS. Driver probe builds x86.
Real-driver execution and real-game performance are separate acceptance gates.

Controlled copy-path comparison (diagnostic build only): startup
`Render/SmallRecordCopy=1` must pass its guard probe. Set
`Diagnostics/SmallRecordCopyLiveControl=1` before launch; the existing one-second
INI poll then accepts `SmallRecordCopyRuntime=0` or `1`. A change logs its actual
QPC application time. Exclude transition windows; keep the camera and scene
fixed. This does not change the startup admission, records, copies, or rendering.
Absent opt-in and production builds retain their original behavior. Invalid
values do nothing, and runtime ON cannot bypass failed startup admission.
