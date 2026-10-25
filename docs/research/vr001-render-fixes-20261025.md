# VR-001 capture and narrow renderer corrections — 2026-10-25

Branch: codex/fix-right-tentacles, based on 6f103a8. No merge.
VR-001 remains OPEN: a native right-tentacle pose problem is not solved by
the independent stereo-transfer correction below. No improved PSVR2 result claimed.

## Runtime evidence

Fresh installed EXE identity/PE/instruction audit passed:
SHA256 d46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1.
Native gameplay/FOV hooks remained suppressed; only the previously authorized
Present callsite observer was enabled. No EXE disk write.

The first request, PID27848/frame15532, acquired both unpaused D3D9 sources,
but XR was not visible and supplied no final-eye submission. Not COMPLETE.
After a user-confirmed restart for bounded geometry/shader acquisition and a
separate user-confirmed headset-ready request, PID480/frame5408 completed.
One request per process, no automatic capture retry.

Private capture: `%TEMP%/Darkness2VR-matched-480-1-5408`.
Capture DLL SHA256 9a3a80be023cd1538e24bc2bc1ec963b086957b5ed80021c47663683028df454.
104 draws per eye; Present serial5409/epoch1 returned D3D_OK with same_witness=true.
`check_matched_capture.py` verified COMPLETE and all seven receipt hashes.
Source RT0:1280x855; final D3D11:2676x2728. These are diagnostic-build dimensions,
not measured production resolution. Original INI request3076x3140 was preserved.
Projection ownership remains unproven; Luna ray-aligned clarity score NOT RUN.

Observed Present prefix:28000 entries/forwards/returns/OK, zero recursion,
statuses, errors or fault. This is prefix evidence, not a headset acceptance.

## Actual renderer correction: shared-texture overwrite during replay

Both final-eye BMPs have identical SHA256:
`c9b50bca2a03061292f9f0488b97feacd862f8d63392ee68394d45fda2638120`.
Source-eye0 and source-eye1 hashes differ. The two final resources are distinct.
Debug mode0, geometric stereo1, baseline0.065, head geometryOFF.

Bilinear resize comparison, RGB code values0..255 (not a ray-aligned sharpness metric):

- Final-eye0 vs source-eye0: MAE4.00712, RMSE11.84738.
- Final-eye0 vs source-eye1: MAE0.07944, RMSE0.28186; 81.6429% exact RGB pixels.

The final eyes carry the RIGHT source. Source inspection identifies a concrete
ordering defect: RenderEyePairFromReplay starts while recording is active.
Its left CaptureBackbufferToShared calls hooked StretchRect, which appends the
mod-owned RT0-to-shared copy to the engine command stream. Replay executes that
extra copy before the next explicit capture's WaitSharedReader. It can overwrite
the shared texture while the left D3D11 draw is still consuming it.

**Implemented:** move recording shutdown to the start of RenderEyePairFromReplay,
before any internal capture/allocation/transfer. Existing explicit transfer fences
are retained. No extra GPU wait, geometry/FOV/IPD/shader change in this correction.
The offline ordering test exercises the delayed-left-read interleaving and asserts
that the real source closes recording before internal capture. A new runtime
capture is still required to establish corrected output on the actual driver.

## VR-001: submission exists; the right pose projects outside the image

Exact runtime shader bytecode was obtained for34D4EEE8,1CB7CEE2,E3259C61.
All skin from BoneMatrices c45 with four blend weights and floor(index)*3,
then apply row-vector WVP c0..3. The earlier c20 companion hypothesis was
REJECTED:34D4EEE8 does not read c20. Its candidate was never enabled and was
removed from final source. Numerical matrix resemblance alone was insufficient.

Right-palette batches correlate with the earlier named DEMONARM root evidence:
translation approximately(0.309,0.413355,-1.034), essentially identity rotations.
Captured stream0 has FLOAT3 position at0, UBYTE4 indices at32 and UBYTE4N weights
at36, stride40. CPU reconstruction of captured stream slices gives:

- Draw2, VS1CB7CEE2, stream offset120400,941 vertices: positive clipW, all
  outside clip volume; minimum NDC Y1.10280. The corresponding historical index
  range15774..19757 references exactly vertex indices0..940 in this captured IB.
- Draw3, VS34D4EEE8, offset158040,72 teeth vertices: minimum NDC Y1.27207,
  all above the top clip plane.
- Draw6, offset285240,442 tail vertices: all clipW negative.
- Left head draw1 has2920/2944 vertices inside clip volume.

These counts describe reconstructed stream slices; the capture did not serialize
all DIP arguments, so they are not a new full indexed-triangle reconstruction.
Both RT0 sources and final eyes visibly lack the right head. Live c45 uploads
are forwarded unchanged by the mod; the live source already has the defect.
The exact matching73EBA724 later pass is MotionBlur_16, not an alternative color
pass: packed BoneVelocities c17..220, motion output, historical B/A-only write mask.

**Remaining uncertainty:** why the engine supplies this right rest pose. The
requested right-slash test was not completed; user reported the game closed.
The next focused control is the same save/scene with the mod absent, then the
right attack/idle transition. Reposing/mirroring the model without this distinction
would hide an upstream animation/state defect, so no arbitrary offset was added.

## Separate opt-in rendering correction: false motion vectors

Full-byte FNV73EBA724 matches existing MotionBlur_16_vs.bin. CTAB proves
WorldViewProjection c0 and PrevWorldProj c4. Its instructions subtract their
projected NDC positions. Captured c0 Px/Py=(0.542956,0.752456), while c4 retains
native(1.156956,1.732051); both eyes share that untransformed c4.

**Implemented:** `[Render] FixMotionVectorProjection=1` applies the same-eye
transform also to the reflected previous matrix. Default0. Gated by exact shader
hash AND current/previous register identities0/4; headGeometry is excluded because
it requires separate previous-head-transform history. Unknown shaders untouched.

Using the captured matrices and a stationary view-space point(0.1,0.1,1):
old pre-clamp motion is(-0.343650,0.642611) left and(-0.625987,0.642611) right;
correcting c4 reduces both components below1.8e-8. Corrected c4 and current c0
differ by at most2.22e-8. This verifies the math, not a measured headset blur fix.
No sharpening or render-resolution change. Yellow pixels remain in source RT0;
their originating pass is unresolved and no speculative correction was applied.

## Validation and restored installation

- Engine Present/witness C++ suite PASS, including full/partial/invalid uploads.
- Matched-capture Python:15 PASS; readiness Python:7 PASS.
- Stereo-transfer ordering/interleaving:3 PASS.
- Motion-vector projection:336 checks PASS; both eyes, old failure, native motion,
  stereo disparity, unchanged Z/W, shader/register/OFF/head-history guards.
- x86 production and DiagnosticCapture builds PASS.
- Fixed renderer Steam/PSVR2 test:NOT RUN. A preflight found active Singularity
  and its PSVR2 bridge; no further game launch or session interruption occurred.

Rollback verified against original backups:

- DLL:9C96DC85F8F0B32C97411D71871A0E2FBBFA27C2818569F986AE78551A72334C.
- Game INI:C802439A7AD84E6B4CEA567405B3A5B85BEEA89EB0A00DBF7313FA5437E77436.
- Diagnostic INI:586F536AFEF7BA596F6F2D8789A74B6B37F20501B4902FD3EAF4D33C5E1DA9C0.

Local INI and EXE hashes also unchanged. Private backup/candidates/logs:
`%TEMP%/d2vr-p0-20261025-103334`. No game assets, raw shaders, logs, meshes,
large traces or DLLs are committed. Production INI is not edited.

## Controlled transfer-fix runtime receipt (2026-10-25 11:30)

After the user confirmed the VR session was free, fresh EXE identity, all audited
instruction sites and function ranges passed. DLL/INIs were backed up before
launch. Diagnostic DLL SHA-256:
`2366c3f2c6a73ec12dd36d637a434a907ac3082a50dd090db93eccdc575ef153`.
Motion-vector correction remained OFF; native FOV hooks remained disabled.

User confirmed unpaused gameplay in the headset. One request produced frame
23138 (Present serial 23139), 242 draws per eye. COMPLETE.json's seven hashes,
frame identities and matching forwarded Present returning D3D_OK passed the
existing validator. Both source hashes and both final-eye hashes are distinct.
Bilinear source-to-final RGB MAE (8-bit values):
- final left / source left: 0.066954; final left / source right: 3.357288.
- final right / source right: 0.066760; final right / source left: 3.358118.

This validates the transfer correction at runtime: each final eye now carries its
corresponding source instead of both finals carrying the right source. It does
not establish physical stereo comfort, camera ownership or a right-tentacle fix.
The unpaused source preview still lacks the right tentacle; its pose/animation
cause remains unresolved. Attack observation and end-of-run rollback are pending.
No automatic capture retry or motion-vector/FOV/sharpness parameter change was made.
Luna ray comparison remains NOT RUN because camera ownership is unproven.
Private capture: `%TEMP%/Darkness2VR-matched-34952-1-23138`.
Private backup: `%TEMP%/d2vr-transfer-test-20261025-113016`.

User physical observation after this capture: executing a right-tentacle attack
made the right tentacle visible, and both tentacles remained visible at the time
of the reply. This establishes that the right tentacle can render through the
current stereo path after an animation/state transition. It does not prove which
native initialization/state producer caused the preceding invisible rest pose,
or establish that the transfer fix alone resolved VR-001. No post-attack capture
was requested. Game was still running when this receipt was recorded; restoration
must wait for normal user exit rather than interrupting gameplay.

Controlled run closed normally by the user. DLL and both game INIs restored from
the fresh pre-test backup; all three SHA-256 values match the baseline listed
above. Installed EXE SHA-256 remains D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1.
Final log and restoration receipt retained in the private transfer-test backup.

## Production-size source capture (frame 108226)

The approved production-resolution opt-in from `31727bc` was exercised after a
fresh EXE/site audit and DLL/INI backup. The running diagnostic DLL SHA-256 was
`b8d63c049d4dab5edbb8d9ac647dcc121acae12fe55bdad3e71767d2e14ba326`.
Only the existing, identity-checked client-size IAT hook was additionally enabled;
native camera/FOV/gameplay hooks and motion-vector correction stayed disabled.

First request 66816 saved both 3076x3140 sources, but XR was not visible and no
final eye images were produced. It was rejected as incomplete. MAIN requested
explicit retry approval, then waited 15 seconds for the user's return to PSVR2.
The single approved retry produced complete frame 108226 / Present 108227:
145 draws per eye, all seven receipt hashes valid, matching arm/entry/return,
forwarded original Present returned D3D_OK. Private folder:
`%TEMP%/Darkness2VR-matched-33700-2-108226`.

- Source RT0: 3076x3140 X8R8G8B8; final D3D11: 2676x2728 BGRA8, both eyes.
- Crop 1,1; linear/clamp blit, full XY viewport. World MinZ 0.05 is a depth remap.
- Both tentacles are visible in the source and final images after the user's
  earlier attack. The yellow doorway region also persists in source and final.
- Current native Px/Py candidate 1.76808822/1.7320509 and GPU scales
  0.307086319/0.434430778 are recorded, not promoted to native-camera ownership.
- Luna's exact UV/bilinear-reference functions were used without assumed camera
  bounds: center 500x500, floor 600x430, ceiling 600x400 ROIs. Across both eyes,
  final/reference luminance MAE 0.0470..0.1078 out of 255; Laplacian RMS deviation
  -0.0413%..+0.0892%. No material additional blur in this transfer step.
  This does not measure source-stage blur, compositor optics or headset acuity.
- Source/final hashes are distinct for each eye. Comparing the wrong source eye
  gives RGB MAE 12.30..23.95 in those ROIs, consistent with preserved eye identity.
- Full ray-compare remains withheld until the active world shader/projection
  proof is independently complete; the inverse-projection candidate is insufficient.

The complete frame additionally confirms the motion-vector mismatch in exact
VS 73EBA724, draws 103..115: uploaded current c0 has the VR projection while
previous c4 retains the flat projection. Undoing the eye transform aligns the
static matrices within approximately 1e-7. Numeric values from draw 103 are now
a deterministic regression fixture; no raw shader/mesh/image assets are committed.
The correction now uses the *resolved current compensation base*, so a CTAB
label cannot enable a previous-only transform when current compensation is
unselected. 401 C++ checks pass, including the real captured matrices for both
eyes. Correction stays opt-in; its Steam/PSVR2 A/B is still NOT RUN.

Optional pass-stage acquisition from `a44969b` is prepared offline, but was not
part of the installed DLL in this run. The yellow-producing pass is therefore
still unproven. No native FOV, crop, IPD, sharpness or shader toggle was changed.
The user is evaluating the high-resolution image; normal game exit and hash-
verified restoration are pending for this run.
