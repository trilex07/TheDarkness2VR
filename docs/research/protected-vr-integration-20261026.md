# Protected VR integration — 2026-10-26

MAIN branch: `codex/protected-vr-integration`, from accepted `85eea69`.
No automatic merge. No Steam launch, deployment or installed-file change in
this development session. The last accepted stereo package is retained.

## Integrated implementation

- `77f6d65`, `33eb527`: geometric head rotation/translation uses metric scale;
  eye translation follows head transform. Deferred inverse projection uses the
  matching basis. Runtime configuration changes revalidate admission each frame.
- SIDE `fbc415f` -> MAIN `e657960`: exact-interface frame-local replay resource
  reference retention, opt-in. Resource lifetime extends through both eyes;
  draw calls, state and required synchronization are unchanged.
- SIDE `6846abb` -> MAIN `0dafa48`: protected external system-XInput entry hook,
  synchronized XR controller snapshots and render-thread recenter queue. Old EXE
  import rewriting is not a fallback. Raw grip maps to stock LB/RB in this profile.
- SIDE `3a18748` -> MAIN `c2e2304`: exact-caller/verified-HWND external USER32
  sizing hook, bounded startup modes and separate actual output-RT0 witness.
  The native size owner creates its dependent resources; we do not resize an
  arbitrary individual render target.
- SIDE `24a3482` -> MAIN `cca749a`: opt-in stock objective/hip reload and
  right-grip planar stroke, with raw RB suppressed until recognition. Native
  targeting/throw/pose calls are bypassed; left grip stays stock LB.
- `f8d00ab`: composed profiles explicitly exclude independent head, gesture,
  quality and performance experiments from the first controller-input test.

All new options default OFF. The accepted protected rendering-only profile is
unchanged. The protected diagnostic path retains main-image write rejection;
new detours target verified external implementation entries, not EXE/IAT/native
static-vtable memory. The production DLL also builds, but the separate diagnostic
DLL is the artifact needed for these protected opt-ins.

## Offline evidence

- Integrated x86 diagnostic and production DLL builds: PASS.
- Actual external XInput ABI/forwarding/image-preservation fixture: 20,032 checks.
- Actual extracted input callback/admission: 30 production, 84 diagnostic checks.
- Actual head/deferred reconstruction/admission regression: 117,021 checks, including saved-lighting policy.
- Frame-boundary/recursion/rollback/capture suite: 2,327,551 assertions.
- Exact-interface retention: 10,000 frames per mode; original owner may release
  before replay and resources remain alive through both eyes; reset/abort/address
  reuse/alias interfaces tested. Synthetic reference-call reductions are not FPS.
- Source sizing: actual x86 external-DLL ABI/caller/HWND/LastError/guard fixture
  and deterministic bounds/aspect/cost sweep PASS. Increased requests do not prove
  actual gameplay source or upstream G-buffer dimensions.
- Protected profile composition: 12 tests PASS.
- Actual protected gesture release/rearm/focus/pause/dwell/stroke regression: 227 checks PASS.

None of these results is a new Steam or PSVR2 acceptance result. Only the prior
`85eea69` rendering-only build has the recorded gameplay/HMD stereo receipt.

## Frozen first test candidate

Private package: `protected-input-c2e2304-20261026T134650Z`.
Diagnostic DLL SHA-256:
`63639ca472555264674930a83d2e1acbc5f9b1b2562b2d041d99465386c6bedf`.
Scope: controller sticks/buttons/triggers/grips and L3+R3 only. Geometric tracking,
gestures, source sizing and resource-reference optimization explicitly OFF.

Fresh read-only checks before packaging matched the installed EXE SHA-256
`d46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1`
and all five installed DLL/INI hashes plus corresponding preserved backup hashes.
The package contains its exact effective INIs and hashes. They must be rechecked
before a later authorized launch; this receipt is not standing authorization.
Before/after engine polling, visible controller response and focus behavior are
NOT RUN. User authorization/Steam availability and physical feedback were pending. The user
then requested head tracking and gestures in the same candidate; that expanded
profile supersedes this frozen input-only package for the next proposed test.

## Exact remaining technical blockers

**Full native 6DoF/visibility:** the implemented GPU view transform changes actual
WVP/depth and corresponding reconstruction, not merely XR layer poses. However,
CPU visibility has already been constructed for the native camera. The old
selected-camera callback at `00986139` is forbidden. No verified external callback
with the selected camera's owner/lifetime before visibility is available. Objects
rejected there cannot be recovered by a shader. Keep this experimental profile
separate; full 6DoF, native collision and peripheral visibility are not accepted.

**Tracked human hands/weapon/support/release:** the admitted arm draw uses 34 GPU
bones. The named 130-bone CPU skeleton has wrists 5/27 and sockets 21/43, but the
indices cannot be equated. Existing ten matched manifests contain no structured
hand upload/overlap witness or correlated remap. Verified `006C3DC4..006C3DCF`
reads the remap from selected renderer +5A8..5AC; resource +488/48C first supplies
batch indices, and `006C3DF8..006C3E20` combines inverse-bind/CPU pose into GPU
matrices. A safe graphics adaptation needs that same draw's renderer remap,
resource batch indices/inverse bind, named CPU pose, actual GPU bank, VB/IB and
controller pose. No guessed bone adapter or unsafe native hook was added.

**Controller-aligned pickup/throw:** protected LB input restores only the stock
action. It does not correct the native candidate selector/aim owner; existing
NativeAim `00C63009` and static NativeHandsPack `00FC2664` remain forbidden.
Button-level gestures cannot be reported as restored ray targeting or throwing.

**Shader first-use stalls:** the existing 97.865ms driver-blocked creation is
retained as evidence. Native cache ownership/loading lifetime remains unproven;
no hash-only shader reuse or asynchronous native shader calls were added.

## User-requested combined candidate

`profiles/protected-controls-head-gestures/` composes protected controller input,
experimental geometric head tracking, and stock gesture actions. Source quality
and reference-retention optimization remain OFF. No unsafe native hooks are
admitted. This expands the originally proposed input-only test; it is not a Steam
launch authorization. The user's answer asked to include head and gestures, and
MAIN has not launched or deployed either package.

Packaging found a concrete configuration conflict: the preserved global lighting
file contains LastKey=7 (inverse reversed eye translation), incompatible with
matched head/reconstruction admission. The new Render.RestoreSavedLighting option
is ON by default, so accepted rendering/input-only behavior is preserved. The
explicit head/combined profiles set it OFF, using configured reconstruction while
leaving the user's lighting file unchanged. Unsafe runtime preset changes still
close admission; the head guard has not been weakened. This is necessary profile
compatibility, not an accepted lighting/stereo fix. Fresh physical fusion and
peripheral visibility must be checked with the new geometry.

Next physical gate is the user-requested combined controls/head/gestures test. Source-size and reference
retention changes need separate controlled comparisons after input acceptance;
head tracking and full hands also require the specific ownership evidence above.
