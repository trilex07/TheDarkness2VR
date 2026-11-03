# Persistent isolated right-eye output, 2026-11-03

## Target and evidence

User accepted retaining Combined and asked for the next larger optimization.
Resolution, same-frame stereo, native FOV/culling, hands/tentacles, camera,
cinema and input remain invariant. No native engine function or EXE patch.

Read-only current Steam hash FE216AF4 matches Combined source d58156b4. A private
copy of its runtime log is test-output/isolated-eye/combined-accepted.log,
SHA2566825156C7A787359AEC333EBB206ECE9A49F203A0DAB0193FACF9DA4E60BE941.
PID36376/TID35328, source2676x2728, native Present returned D3D_OK. At frame8400:
material forwarded6055464/skipped499602/unowned0; Present overlap5821/plain2579,
8 overruns; waitEMA1.871ms/PresentEMA0.310ms. Packet worker at frame5400 reports
prepass0.1059ms/join0.0001ms. A nearby stereo phase window: replay/history0.57ms,
right/restore2.13ms. GPU live2.595/replay5.641ms are timestamp SPANS including
submission gaps, not GPU Busy or recoverable savings. These logs do not supply
a controlled before/after FPS comparison. They argue against further expansion
of the tiny worker and locate the next structural work in output scheduling.

The existing scheduler makes four full-surface copies besides the two eye
captures: left backup, right history seed, right history save, left restore.
At2676x2728 each32-bit image is29,200,512bytes. Four copies write116,802,048bytes
and read the same amount per admitted pair. This is traffic removed, not a
bandwidth-derived time/FPS promise. The retained shared-capture reuse only
removed two copies, was quarantined after right-eye artifacts in a combined
trial and remains OFF. This experiment does not enable that path.

## Change

IsolatedEyeOutput=1, startup-only/default0, retains a private RT0 for the right
output instead of drawing right into the native left output. Same dimensions,
format, pool and sample count; single-sample X8/A8 only. One-mip texture-backed
outputs retain both surface and base-texture identity and create a matching
private texture; standalone RT/backbuffers create a private RT. Unsupported
container, mip, format, sample and allocation results reject the path.

The sealed frame's resource pointer fields are temporarily substituted in
SetRenderTarget, SetTexture, StretchRect and ColorFill records. Opcodes, payloads,
draws and order remain unchanged. All patch storage is assembled before any
record is modified; failures leave original records intact. Original resource
refs remain in the frame ledger; the output owns new COM refs. The recorder
generation and exact field values must match before restoration. RAII restores
on error and latches overflow if foreign changes prevent exact restoration.
Only owned CPU packet data is visible to the existing worker.

Frame-start Snapshot Apply substitutes RT identities BEFORE applying its native
state block, preserving the captured viewport. Inherited pixel/vertex textures
are remapped after Apply; unsupported stage caps are respected. Live-end Apply
is ordinary and uses original identities, after records have been restored.
Recorded opaque StateBlock Apply, MRT reuse of this output, output content
updates, matched image capture and light-input readback reject this candidate.
The original scheduler is the fallback; no validity/ordering gate is bypassed.

First use/fallback seeds from the exact latest right history, or native live
pixels under the existing first-segment/fresh-world recovery contract. Steady
pairs keep partially drawn/UI background in place. Successful output is adopted
as right history; fallback Save detaches that alias. Failed replay, restore or
transfer invalidates candidate/history; UI-only segments cannot resurrect a
rejected image. Original live RT0 pixels remain untouched by output rendering.
Shared intermediates and depth follow the accepted serial replay as before.

The feature requires single-owner transfer mode4, split shared inputs, reader
fences and zero-copy captures; ReuseSharedCaptureColor must remain0. It still
copies each finished eye into its own shared input and retains all producer
END/FLUSH, reader-retirement and XR Wait/Begin/End ordering. It does not submit
the previous frame, move D3D9 calls to another thread or change device flags.
Both Reset paths release the private output/history before native Reset.

## Validation and limits

- Standard and protected-native x86 /O2 builds PASS.
- Real D3D9Ex HAL flags0x50:101169 checks (/W4 /WX) using actual recorder declarations,
  actual record patch and actual Snapshot Apply. Standalone/texture outputs,
  inherited pixel/vertex texture aliases, viewport, partial/UI/full redraw,
  original left pixels and complete right-pixel equality at64x48 and2676x2728;
  failure/opaque rejection, RAII, stale field refusal, resize/recreation and Reset.
- Extended real D3D9Ex/D3D11 fixture PASS2860 eye transfers,1080 full resolution,
  including100 transfers from persistent isolated output, independent shared
  readers, deferred fences, UI persistence, both-eye sampled pixels and Reset.
  This is genuine interop execution, not the complete game's pair scheduler.
- Actual legacy history1000 partial/UI+1000 rejection segments PASS; actual
  snapshot failure receipts2545 PASS; surface/status128 streams/896 calls PASS.
- Both-flavor actual float consumer384 streams/2304384 records/224468 checks,
  binding failure/order, protected head117021,47/130 hands and tentacle27099 PASS.
- Existing frozen installer eight-mode exact restore/busy/corruption fixture PASS.

No game launch or Steam write. FPS/HMD: NOT RUN for this experiment. Removing
copies can shorten the producer critical path, but the current game saving and
eligibility frequency are unmeasured. This does not solve native object/material
parallel production or claim the whole2ms right/restore scope as a saving.

## Frozen test candidate

tools/prepare-isolated-eye-candidate.ps1 freezes the committed protected build
with accepted Combined DLL/INI as exact rollback. Default Combined retains ALL
accepted optimizations and adds only IsolatedEyeOutput=1. Off uses the same new
DLL and disables ONLY IsolatedEyeOutput; it does not disable accepted Combined.
Install.ps1 -Restore returns exact accepted FE216AF4 DLL and2537EC1E INI.
Only preparation is authorized in this task; game remains on accepted Combined.

Next gate: explicit closed-game test window, then verify both eyes in natural
combat, pause/UI-only segments, cinema, hands/tentacles and transitions. Collect
IsolatedEyeOutput active/fallback and compare identical scene A/B at unchanged
resolution before deciding whether it improves performance. Keep rejected
shared-capture reuse OFF. No main merge by inference.
