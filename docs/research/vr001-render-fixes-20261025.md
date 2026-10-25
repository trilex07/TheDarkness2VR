# VR-001 capture and narrow renderer corrections — 2026-10-25

Branch: codex/fix-right-tentacles, based on 6f103a8. No merge.
VR-001 remains OPEN for the initial invisible rest state. Both tentacles after
an attack and improved high-resolution clarity are now physically confirmed;
the receipts below separate those observations from untested corrections.

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
The user subsequently confirmed in PSVR2: tentacles, hands and gun are sharp;
the near area is acceptable and distant detail is improved, with more resolution
requested. This is a subjective physical result, not a controlled sharpness A/B.
After normal user exit, all three original DLL/INI hashes were restored and
verified; EXE SHA-256 is unchanged. Final log and restoration receipt remain in
the private high-resolution backup. Present counters through 147000 had equal
entry/forward/return/OK counts and no recursive dispatch/status/error/fault.

## Prepared source-sized eye output

`[Render] MatchEyeResolutionToSource=1` is a new opt-in (default OFF), limited
to same-frame native stereo. The measured 3076x3140 source was being reduced to
the runtime recommendation of 2676x2728 before submission. The option preserves
the actual source dimensions through final D3D11 and XR images: approximately
32.3% more output pixels for this installation, without a FOV/crop/IPD change.
It only raises both dimensions together; smaller, unknown, over-budget or
runtime-unsupported sources retain the previous recommendation. Limits use the
minimum maximum dimensions of both XR views. The x86 experiment is bounded to
16M pixels per eye.

Sizing reads the current RT0 rather than stale prior-frame upload dimensions.
The completed pair's allocation is latched; SubmitXR cannot recreate a differently
sized swapchain and erase completed eye images at a first frame/reset. Both
capture and submission report their actual dimensions. Other submission modes
and default OFF retain the previous resolution policy.

Offline: output-size boundary cases and 1206 existing rendered-FOV rays PASS;
20 matched-capture tests PASS. Production and diagnostic x86 builds PASS.
Steam/PSVR2 for this output-size option are NOT RUN at preparation time.
The next diagnostic variant also contains the existing bounded pass images for
yellow-region localization. Motion-vector correction remains OFF to isolate
the resolution change from a separate rendering correction.

## Source-sized output runtime receipt (frame 38224)

Candidate 3be6a54 ran with diagnostic DLL SHA-256
F6D617F200CF7975109FB5384FB8C699C9B17FB0363AA75025F6A4E7DEC13337.
One requested unpaused gameplay frame completed, Present serial38225, 122 draws
per eye. All seven COMPLETE hashes and every optional shader/stage hash pass.
Private capture: `%TEMP%/Darkness2VR-matched-36500-1-38224`.

Sources and final eyes are all3076x3140. Every RGB component in each entire final
image equals its corresponding source exactly (MAE0, max error0, no differing
components). Both tentacles are visible in these captured images after attack.

Actual saved VS12CCC8DB and293F456B bytecode declares row-major WVP c0..c3 and
uses its x/y/z mul/mad chain plus c3 to write o0. World draw14's uploaded matrix
has projection basis ratios Px=.542955717354, Py=.752456179471; normalized
x/y/w cross dot products are below4.3e-8. Same ratios are present in both eyes;
their translations differ. Full viewport and identity crop establish the image
ray mapping independently of the native inverse-projection candidate. Native
camera object ownership remains unproven.

Luna ray-compare runs through the receipt checker, using this rendered-source
frustum, pass for left wall ROI950,650,500,500 and right floor
ROI1100,1900,600,430. Max submitted-FOV/crop tangent error2.01e-7; aligned
pixel MAE/RMSE/max0, gradient and Laplacian RMS unchanged. These results rule
out extra loss in this application's RT0-to-eye transfer for this frame, not
source-stage filtering or compositor/optics effects.

User's physical result: blur still remains, including while fully stationary.
Resolution alone has not fixed VR-003. Known DOF/FXAA bypass creation succeeded
in the log. Motion-vector correction was OFF. No headset sharpness success claim.
After normal user exit, original DLL/game INI/diagnostic INI SHA-256 values were
restored and verified; EXE remainsD46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1.
Restoration receipt: `%TEMP%/d2vr-eye1to1-20261025-122411/restoration-verified.json`.

Seven bounded intermediate targets were saved. Full HDR color after draw75 and
half-resolution color after94 do not contain the bright yellow doorway; final
source RT0 does. This narrows its introduction to later postprocessing. It does
not prove CPU culling, absence of submitted doorway geometry, or which later
shader is responsible. The normal buffer's neutral doorway value alone is not
an object-rejection witness.

## Narrow material-sampling correction and next controlled candidate

Historical recorded stream run43052/frame003 contains VS293F456B / PSCA08BDC4
draws with s0.MINFILTER=LINEAR (3 draws) as well as ANISOTROPIC (8 draws),
with s0.MAXANISOTROPY16. The maximum alone cannot promote LINEAR sampling.
New actual saved PSCA08BDC4 bytecode independently declares DiffuseMap=s0 and
samples material UV there; world VS293F456B writes WVP-transformed position.

`[Render] WorldDiffuseAnisotropy=1` (default OFF) now promotes LINEAR minification
only for that exact world shader pair, s0, a mipmapped non-RT/non-depth 2D
texture at least64x64, and supported driver capabilities. Existing anisotropic,
point, unknown, postprocess, lightmap, normal-map and UI paths are unchanged.
MAXANISOTROPY is hardware-clamped. Both sampler values restore after each draw;
the same scope runs in live and replay and adds no recorded sampler events.
LOD bias, mip/mag filters and all texture contents are preserved. Capture records
the actual per-draw filter and whether this scope applied. This is a bounded
candidate for distance-texture loss, not a proven explanation for all blur.

Real D3D9 HAL regression:424 checks PASS, including exact shader exclusions,
every sampler state on s0/s1, repeated eye/draw scope, stateblock restoration,
unsupported inputs and partial setter failure rollback. No Steam or XR used.
Production and diagnostic x86 builds PASS; motion401, matched-capture21 and
transfer-order3 regression checks PASS. Physical material-sampling A/B pending.

`[Diagnostics] CapturePassStages=2` reuses the existing one-frame capture and
8-attempt/256MiB limits but reserves them for the late postprocess interval.
It saves the exact final composite's s0 Frame/s2 Depth/s3 DOFScene inputs before
its draw, c0..c15 PS constants, actual shader bytecode and later target output.
R32F depth is explicit, with no format conversion or unsupported fallback.
Input/output phase and sampler identity are distinguished and validated. Mode1
retains general pass capture; default0 still has no GPU readbacks.

## Diffuse candidate runtime result (frame28216)

Candidate d19f552 diagnostic DLL SHA-256
818E7E117E13F63DF9BE6849EE9210C38B5E246F7081CAF0A1E1131A7BC425E4
completed one unpaused 3076x3140 capture in each eye. COMPLETE and all optional
hashes pass; Present28217 forwarded once and returned D3D_OK. Private capture:
`%TEMP%/Darkness2VR-matched-32704-1-28216`.

Both entire final-eye RGB images again equal their corresponding RT0 exactly.
The exact material pair has six draws per eye. Actual s0 minification is
anisotropic16 for all six; the scope corrects only draw30, because the other
five already use anisotropy. No universal texture/filter change occurred.
The user's PSVR2 assessment was "maybe slightly better"; VR-003 remains open.

Late-stage capture now locates the yellow doorway's appearance at final
composite draw135: the full-resolution HDR Frame input precedes it; the bright
yellow region is present after it. Saved actual PS A69BDC80 (declared33DE98C1)
contains the intended mov/nop replacement of the DOF/ghost RGB blend. Therefore
failure to activate this bypass is ruled out in this frame. Height-fog, bloom
and grading still execute. This does not establish why underlying doorway
geometry or depth is missing, nor prove CPU culling.

Depth s2 was not saved. The previous capture silently skipped unsupported
surfaces, so its type/readback reason is UNKNOWN. The next diagnostic reports
all three input identities and explicit availability/readback status, including
unsupported formats, without inventing a depth conversion. OFF-by-default
CameraUploadWitness records at most four original world VS293F456B c0 uploads,
their API result and OS stack addresses through the existing COM hook. It
does not dereference candidate camera objects, alter matrices/FOV, or certify
camera ownership; it is compiled out of the production build.

Normal user exit was followed by verified DLL/game INI/diagnostic INI restoration
to 9C96DC85 / C802439A / 586F536A respectively (full baseline hashes above).
EXE SHA remains D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1.
Private restoration receipt: `%TEMP%/d2vr-diffuse-test-20261025-130050`.

## Mono upload witness: stopped, baseline restored

Candidate c0f680e diagnostic DLL SHA-256
4159D879CE3AF1D7D53F5FB9C34B78B736C670440C4CE9D266F70A38F2229927
ran mono, OpenXR OFF, capture OFF and native FOV OFF. Installed EXE D46BB6CB
passed the fresh identity/byte audit; no other game was running. The user loaded
gameplay and closed the game normally after the stop request. No stereo capture
or headset acceptance took place in this trial.

Four original VS293F456B c0/count12 uploads at frame75761 returned D3D_OK.
Their source pointer was0EC56F80, original caller return00B74740 and OS stack
included00B74740 and0071F71F. This is an actual engine upload witness, not an
inference from a shader name. Static dataflow is recorded in the native-camera
audit below; active camera ownership and the selected live semantic remain open.

The final sampled Present prefix covers155000 entries/forwards/returns/OK,
with recursion/status/error/fault all0. However, a render-thread first-chance
E06D7363 occurred; `check_present_readiness.py` returned **NO_GO**. Two further
first-chance notifications on another thread include UIAutomationCore during
shutdown. Counter success does not waive these observations or prove all returns
through exit. The game continued after the first notification; this is not
evidence of an unhandled crash. No automatic relaunch followed.

Focused byte-verified disassembly found an explicit native RaiseException/SEH
bridge: EEA420 -> EEA5A0 -> imported RaiseException, with a local SEH chain and
EEA5F0 consumer. The static template107B7A0 carries E06D7363, count3 and
91930520; the first saved stack contains91930520,00EEA1D7,015EEB0C and return
EEA5E6. This strongly supports that bridge as the source. The old logger omitted
the actual ExceptionInformation fields, so the runtime record and consumed
continuation are not independently proven. The NO_GO result is retained.

The diagnostic build now logs the OS record's flags/count and at most15 numeric
information values, without dereferencing any throw object, ThrowInfo or nested
record. Its header is compiled out of production. A real locally caught Windows
exception and boundary/invalid-address tests pass22 checks. Present readiness
passes8 tests, including retention of NO_GO with the new metadata; native audit
tests pass13. Both x86 production and diagnostic builds pass. No new rendering,
shader, FOV, stereo, filter or installed-INI change was made in this follow-up.

At13:30:51 local, DLL/game INI/diagnostic INI restored with full baseline hashes
9C96DC85/C802439A/586F536A (prefixes; full hashes recorded above). The current
installed EXE still hashes D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1.
Private run, full log, checker output and restoration receipt:
`%TEMP%/d2vr-camera-mono-20261025-132419`. Further Steam testing remains stopped
after this NO_GO; the exception witness has been built offline, not deployed.

## One approved mono test of679ef63: exception origin identified

The user explicitly approved one mono/OpenXR-OFF/capture-OFF test. Fresh
EXE SHA/PE,26 site and18 range checks passed. No other Steam game was active
(only SteamVR/fpsVR). All three installed files were backed up and verified.
Diagnostic DLL7C0C066765BA67E54D9E4B427A662DA07A20EECF9936E8965D857CAD9AD98A3C
was deployed once, using the preceding mono test's unchanged INI files.
Steam started PID7452; effective log confirmed native gameplay/FOV hooks OFF.
Desktop observation showed loaded, unpaused barber-room gameplay; no XR ran.

The render-thread record is now explicit: E06D7363, flags1, parameters3,
nested0, information[0]=91930520, [1]=00EEA1D7, [2]=015EEB0C, thread32312.
The full EXE template107B7A0 contains the same code/flags/count/marker.
Native outer scope EEA6D0 installs EEA5F0 through EEA2F0 before calling
EEA190 -> EEA480 -> EEA420 -> EEA5A0 -> imported RaiseException atEEA5E0.
EEA420 deliberately passes the saved return address and its stack destination;
EEA5F0 writes information[1] to *information[2]. The observed EEA1D7 is exactly
EEA190's return from EEA480. This identifies a native exception-based return-
address handoff, rather than a throw object guessed from EBX. No patch or
exception suppression was applied. The precise handler instruction was not
single-stepped, but later successful Present returns prove the process resumed.

Two additional E06D7363 records are on a different thread35396, with ordinary
19930520 markers and UIAutomationCore stack addresses. Their exact type/catch
path is not established; they are not reclassified as harmless renderer events.
The conservative checker remains **NO_GO**. The test stopped and was not retried.

Thirty sampled result rows cover27000 entries/forwards/returns/OK, with
recursion/status/error/fault all0. Detach presentCount27857 is not a complete
HRESULT aggregate, so do not claim full-session return coverage. No crash was
observed; the user closed normally. At14:17:57 Europe/Warsaw all original DLL/
game INI/diagnostic INI hashes were restored (9C96DC85/C802439A/586F536A);
local INI and EXE stayed byte-identical. Private evidence, final log and receipts:
`%TEMP%/d2vr-exception-mono-20261025-141446`.

Read-only audit adds the outer scope and six complete handoff ranges to make
this provenance reproducible. It does not change the readiness classifier,
production rendering, FOV, stereo, shaders or installed configuration. No new
capture or PSVR2 test occurred; VR-002/VR-003 are not visually fixed by this trial.

## Targeted composite rendering change after the mono investigation

`DisableCompositeHeightFog=1` now replaces the exact PS33DE98C1 height-fog
blend (`lrp_pp r1.xyz,r0.x,c5,r3`) with its input scene RGB. It is OFF by default
and needs a restart, like the existing DOF/FXAA overrides. It does not change
depth reconstruction, native FOV, visibility, lighting, stereo matrices, bloom,
color grading or texture sampling. Both live and replay bind the same created
replacement shader. If the driver rejects it, the existing DOF-only replacement
is tried before the original game shader. There is no new native hook.

This is an actual, narrow rendering A/B candidate prompted by complete
frame28216/draw135, not proof that height fog is the sole source of its yellow
region. Removing fog may expose a dark gap; missing geometry remains a separate
unresolved issue. The effect is removed across this composite, not selectively
masked by screen coordinates or pixel color. No distance-detail acceptance is
claimed: the prior full-resolution source-to-eye transfer was already exact.

The new shared bytecode patcher verifies full original length/hash, unique
instruction boundaries and operands; it ignores CTAB/comments and retains all
other tokens. Existing DOF-only outputs stay A69BDC80 (33DE98C1) and D39791EC
(75F88E11). Fog is supported only on the witnessed33DE98C1, producing88F23DC1
alone and C0A6F934 with DOF bypass. No shader dumps/assets are committed.

Offline validation:2496 checks PASS using a real D3D9 HAL fixture plus both
private original game shaders. Driver accepts every supported replacement;
GPU pixel readback verifies independent DOF/fog controls, retained downstream
gain/offset and alpha, and repeated-eye consistency. Synthetic red-channel
contrast is16 with fog versus80 without; this is a fixture result, not a game
or headset measurement. Default-off historical bytecode identity is checked.
Matched-capture regression24 PASS. Production/diagnostic builds are checked
separately. The option is included in effective capture metadata/config identity.

No Steam launch, DLL/INI deployment or PSVR2 test was performed for this change.
The previous installation stays restored. The next useful physical test is
this concrete composite variant; another mono exception investigation is not
needed to evaluate its image. Earlier NO_GO is not silently reclassified or
used as authorization for a new launch.
