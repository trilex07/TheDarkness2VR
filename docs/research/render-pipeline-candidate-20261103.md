# Combined render-pipeline candidate, 2026-11-03

## Delivered scope

One frozen, opt-in candidate combines prior shader-demand caching, the owned
full replay packet worker, new native material scalar suppression, an earlier
packet launch, and the retained native Present overlap. No resolution, FOV,
eye ordering, transfer mode, camera, hand, tentacle or gameplay policy changes.
The tiny ReplayPrepareWorker stays OFF in Combined. All new switches default
OFF in source; preparing the package does not modify Steam.

This is NOT full parallel preparation of native object/material lists. That
part of the requested second direction remains unimplemented: the native
producer boundary and immutable ownership have not been established. See
`combat-critical-path-search-20261103.md`: EXE 006C74A0 submits mutable geometry;
0073F570 flushes mutable batches; 00B4D2B0 selects shaders and mutates shared
caches. Existing numerical native jobs already use workers. Moving these
functions wholesale would violate D3D9 ownership. No large FPS gain is claimed.

## 1. Fewer driver calls

NativeMaterialFilter adds an allocation-free owner-epoch ledger for eligible
public RenderState values and sampler states. It requires NativeSubmissionFilter,
the exact admitted device/thread and active successful recording. Repeated exact
values are suppressed only after D3D_OK AND successful command publication.
Errors, S_FALSE, unknown/vendor states, reentry, missing publication, raw state
scopes and stateblock boundaries reject reuse. POINTSIZE/RESZ/ATOC commands are
always forwarded. OFF retains the previous hook behavior.

The existing native VS/PS constant and geometry-binding filters remain enabled
as in the accepted profile. Textures and depth bindings still forward: no new
texture lifetime contract is asserted. Render order and draws are unchanged.
World-diffuse raw sampler promotion/restoration invalidates the ledger on both
sides; explicit Restore is idempotent and prevents a second destructor write.

Actual-hook synthetic fixture: 4096 repeated scalar commands -> 512 native
forwards, 10708 checks. This is a call-reduction fixture, NOT measured game
frequency or a frame-time reduction.

## 2. Earlier owned replay packets

EarlyPacketPreparation seals/copies the recorder under its lock and starts the
existing FullPacketPrepareWorker before live-end capture and color backup.
The worker processes owned VS constants, shader descriptions, transforms and UI
corrections; no native object pointers or COM calls escape the render owner.
An early job starts with UNKNOWN inherited shader because the live-end shader
is not frame-start state. Only explicit shader commands certify later packets.
Exact stereo parameter matching and existing per-packet shader/config/FP checks
guard reuse. Ordinary owner computation remains the fallback; a lease joins on
every error/early return. This does not remove D3D9 submission from its owner.

Full-packet actual-source suites PASS 942915/942916 checks, two build flavors,
four FP rounding modes. Synthetic 2048 packets: serial ~0.203ms, worker including
submit/join ~0.185ms (input already assembled). Owner prepass ~0.070ms and consumer
~0.040ms in that fixture. These small numbers set expectations; early overlap
has not been timed in the current game.

## 3. Fill an existing wait

Combined enables retained OverlapNativePresent: the owner can forward exactly
one native monitor Present after right capture, color restore and live-end Apply
are queued, while the right producer remains pending. Prediction requires
32 warm-up samples, a 0.25ms margin and 90-frame backoff after a miss.
EXE identity, thread/device and submission mode are guarded. No producer FLUSH,
reader retirement, frame/eye dependency or OpenXR sequence is removed or moved
to another thread. Historical mirror cost ~0.36ms inside ~1.4ms waiting is an
opportunity estimate, NOT current savings. Details: HANDOFF-present-overlap.md.

## Validation

- Standard x86 /O2 build and protected-native x86 /O2 build PASS. Fixed the
  pre-existing missing non-protected ProtectedShotgunSupportOwner definition
  with the same false/null stub convention; protected behavior unchanged.
- Native scalar cache and actual native ownership boundaries PASS; actual
  live hook suite10708, replay state receipts184 and stateblock scopes PASS.
- Actual packet lifecycle/fallback942915/942916, tiny worker65589 and cache8405
  PASS. Actual float replay consumers PASS both flavors:384 streams,
  2304384 records,224468 checks; replay bindings PASS.
- Protected head117021, cinematic47/130-bone restoration and tentacle scope27099
  PASS. These are offline checks, not physical headset acceptance.
- Real D3D9Ex HAL material sampler479 checks PASS, including failure rollback,
  stateblock and explicit repeated Restore. Legacy D3D9 software-VP fixture is
  BLOCKED at device creation0x8876086A; hardware-VP legacy0x8876086C. The added
  explicit -Ex test uses DEFAULT textures (Ex cannot use MANAGED), hardware VP;
  it does not silently turn the legacy failure into a pass.
- Present policy435138 and actual x86 forward-once observer5000 calls PASS.
- Real x86 system D3D9Ex/D3D11 transfer2760 eyes,1060 at2676x2728 PASS, including
  native single-owner flags0x50, reader fences, pixels, failures and Reset.
  Present-contract4 and transfer-order7 (11 total Python tests) PASS.
- Eight-profile installer fixture PASS: exact restore, active-game/corruption
  rejection; real Steam untouched. git diff --check PASS.

Private evidence: MAIN worktree test-output/combined-*.log,
test-output/overlap-combined.log; standard artifact backup in
build/render-pipeline-standard-20261103. Frozen package identity is recorded
in the per-task handoff after generation.

## Package and acceptance

Run tools/prepare-performance-candidate.ps1 -DefaultMode Combined only from a
clean committed tree. Combined enables ShaderDemandCache, FullPacketPrepareWorker,
EarlyPacketPreparation, NativeMaterialFilter and OverlapNativePresent; disables
ReplayPrepareWorker. Other eight-profile choices isolate Material, Packets,
Overlap, Cache, Worker, Both or Off using the same DLL. Off is not the old DLL;
Install.ps1 -Restore restores exact accepted DLL/INI backups.

Baseline receipt: cinematic-theater-20261102T193328001; deployed DLL
B97B44775EAC979221FBAF9476F9A6D568B01B48681935B1A1A1CCE7DE926BC2.
Packager checks every protected target and preserves all other INI text.
Runtime FPS and PSVR2 acceptance: NOT RUN. Next gate is one controlled combat
A/B at identical resolution/location/settings, with exact package/profile
identities and natural enemy activity. Check both eyes, hands/tentacles and
camera behavior before accepting performance. Do not merge to main by inference.
