# Architecture review: integration decision and next gates

2026-10-24. Independent source/diagnostic review; no runtime acceptance implied.
Base: `research/evolution-engine-render-pipeline` at `cb6638fc1ce429eb0b85177b5b1a33261f1f74d2`, never `main`.
All three remote tips and the base were verified against origin. No merges, native hooks, Steam changes or DLL deployment were performed.

## Decision and exact integration order

1. **Luna — PASS for offline diagnostics/documentation only.** Cherry-pick, in order:
   - `744e63733c6de095b999de99846649f6334e64e2`
   - `accd9b24c5b81c402395bdeae2ccaaae632d0f39`
   Apply the interpretation limits below; these commits do not prove the cause of blur or establish metric world scale.
2. **Sol — PASS for static evidence tools/documentation only.** Then cherry-pick:
   - `f978db8dab0cf64ee14d77eba7eeb3af71b2f5b8`
   - `5f59fe1a9bb4b916f1c0a93450d4da7d889672e3`
   Keep `runtime-experiment.json` disabled. Native implementation remains **CONDITIONAL** on camera ownership, ABI, units and visibility proof.
3. **Gemini — REJECT the current adapter for integration as authoritative evidence.** No current commit is approved for cherry-picking. Hold the entire chain `ae8de22 -> 0f9085f -> 85be172e1a845dbb28e4172ae5d115e7f4d4ff1f` until the blocking corrections below are reviewed. External-tool notes remain reference material, not a reason to replace the proxy or start another tool survey.
4. Retain this review alongside the accepted documentation, then implement only the diagnostic experiment below. A corrected Gemini series may follow later; do not pick its last delta alone.

**Conflict assessment:** relative to the pinned base, the three series change disjoint paths; none changes production `src/` or INI. No inter-series textual conflict is expected from those diffs (no trial merge/cherry-pick was performed). Each tip depends on its earlier commits. Sol adds tracked `MODLOG.md`; the original working directory already has an untracked file of that name and other local work. Integrate in a clean worktree and preserve/reconcile that local log separately. Semantic conflicts are Gemini's camera/staleness claims and Luna's scale interpretation, not overlapping patches.

## Critical findings

### Gemini: state provenance is not yet trustworthy

References below are to `tools/external-toolchain/trace_matrix_adapter.py` at `85be172`.

- **P1 — `TraceAnalyzer.parse_file`, lines 414–482: VS/PS bank contamination.** Every `cN` line writes `self.constants`, regardless of its owning opcode. Production `src/d3d9.cpp`'s eye trace writer emits those lines for both `OP_SetVSConstF=20` and `OP_SetPSConstF=27`; only VS writes update adapter provenance. Independent fixture: VS c0=(1,2,3,4), then PS c0=(9,8,7,6), leaves the adapter's alleged VS value=(9,8,7,6) with the old VS upload command. Split banks, bind data lines to opcode/start/count, and mark omitted values unknown. Never combine new provenance with old values.
- **P1 — `process_draw_call`, lines 295–313: cross-pass does not mean stale.** D3D9 constants persist across RT changes. A pass change is context, not a register write. Preserve values until overwritten/restored/reset; report upload-pass and draw-pass separately. `current_pass_epoch` is not used for register provenance, and equal-sized/format RTs collapse into the same category anyway. Track RT identity and viewport, not just dimension presets. RT0 slot isolation is a useful correction; RT1 changes must not redefine RT0. The current cross-pass unit test enshrines the incorrect rejection rule. See Microsoft's [SetRenderTarget contract](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-setrendertarget), which also resets the viewport.
- **P1 — `parse_file`: unknown shader/state restores retain unsupported certainty.** `op=19` without a following VS metadata line leaves the previous VS active in the analyzer. `OP_ApplyStateBlock=36` is ignored. Clear unknown bindings and invalidate/reconstruct affected state when a restore cannot be decoded; require a known initial-state snapshot. D3D9 [state blocks restore device state](https://learn.microsoft.com/en-us/windows/win32/direct3d9/saving-all-device-states-with-a-stateblock).
- **P2 — `process_draw_call`, lines 315–380: layout and identity are overclaimed.** Trying raw data first and transposing only when solving fails does not disambiguate two plausible layouts. Establish layout from shader use/reflection, or retain ambiguity. Normalize the adjacent model consistently; comparing only the maximum upload command does not prove all eight registers came from one complete upload. W=I plus matching Px/Py and RT dimensions establishes a projection-consistent VP candidate, not the active player camera. Rename `VALIDATED_CAMERA_WVP` accordingly until ownership is witnessed. An auxiliary camera can have identical projection. Plane angles/apex from WVP are in input/object space; rigid object rotation preserves angles but changes coordinates, and nonuniform scaling can distort them.
- **P2 — `parse_file`, large-upload quarantine:** `count>16` blocks every register regardless of start, although the report describes high-register bone palettes. This avoids sliding-window false positives but can reject a legitimate bulk constant upload containing WVP. Require CTAB/shader-use evidence for bones; otherwise label bulk/unknown. Hardcoded 3076x3140 and Px/Py tolerances are capture-specific heuristics. Format 23 is R5G6B5, not the R32F claimed in the shadow note (R32F is 114).

Minimum correction tests: persistent VS constants across RT0 changes; same-size different RT0 identities; PS writes leave VS unchanged; unknown VS binding; state-block restore; missing bulk payload; partial uploads; ambiguous transpose; auxiliary camera with identical projection; true bone palette versus bulk WVP. Existing passing tests are not an accuracy certificate.

### Luna: valid image mapping, limited calibration claim

`tools/clarity_capture_analysis.py::{make_ray_aligned_reference,make_crop_resample_reference,compare_ray_aligned}` correctly maps target pixel centers through tangent bounds and uses source coordinates `u*width-0.5`, with bilinear/clamp sampling. Centered crop prediction supports asymmetric bounds. It assumes a common camera orientation/origin and full image viewport: it cannot remove stereo parallax, a pose change or a non-full viewport. Its 8-bit grayscale baseline is an approximation to GPU color processing, not bit-exact HDR/sRGB validation. Compare matched color encodings and include quantization tolerance; retain color originals.

For parallel translation, `d = W * P_eff * I_game / (2*Z_game)` is correct, where **Z is axial view depth**, not Euclidean range for an off-axis feature, and `P_eff=P_native*scaleX/cropX`. Signed shear disparity is `W*StereoSep*(1-C/Z)` before any output crop; divide by cropX when applicable. The C++ shear fixture uses crop=1. Unequal eye intrinsics require projecting into each actual eye separately.

**Scale caveat:** `measure_horizontal_disparity` and `TestScaleRecovery` recover the baseline encoded in the image. Dividing that baseline by XR IPD gives the scale implied by current settings; it does not independently measure the world's intended metres. Do not use this circular result to validate `GameUnitsPerMeter`. Require an independently known physical/engine prop size or metric distance. Use distinctive static ROIs, check polarity/occlusion and match ambiguity, and enlarge the integer search window when needed (default 80 px misses the example's ~154 px at Z=1). Sharpness and weak depth remain separate hypotheses.

### Sol: useful primitives, no certified native camera hook

`docs/research/native-camera-fov-audit.md` records the following static evidence for depot SHA `B56B5998...`; this review inspected the report and guards, not a fresh disassembly of the private binary:

- `0x00A31FF0`: sixteen perspective-shaped stores, ECX output, `ret 0x14`, degree half-angle factor; the exact `0x00E1BC92` math routine remains unvalidated. Caller `0x0044609F` consumes object +17C/+184 and feeds multiplication `0x00C02F50`. This is the strongest next lead, not identified player-camera ownership.
- `0x00DF3B50`: similar perspective structure, ESI output/XMM0 far argument; caller `0x0096ED99` stores into +480. Custom register ABI must not be replaced by a guessed thiscall signature.
- `0x00DF53E0`: eight corners, not six planes. Caller `0x0070CB42` uses +17C/+184, then reduces corners into min/max bounds. Bounds construction is not the CPU visibility reject test.
- `0x007BF850 -> FCOS(raw_slot270/2) -> dot comparison` supports cone/candidate selection, not projection construction. FCOS consumes radians, but this does not establish the getter's intended units or camera role. Getter/setter bindings use different type globals (`01100984`/`01100994`); inheritance/conversion remains unresolved. Never call setter slot E4 on a cached slot-270 receiver based on a plausible numeric range.

`tools/native-camera/audit_camera.py` rejects the depot variant despite matching six sites; preserve that fail-closed identity policy. A restored EXE needs a new explicit audit, not automatic allowlisting. `CameraEvidence.java`'s byte assertions/completion marker help bound static evidence, not certify a hook ABI.

## Shortest remaining static path

1. Slice backward from `0044609F` through `00445E40`: identify the +17C/+184 owner via constructor/vtable and the active-camera selector; trace writers of angle/aspect/near/far and units. Resolve `E1BC92` narrowly. Do not begin with a global search of every GetFOV call.
2. Slice forward from its output through `C02F50` into the exact recorded world VS upload and corresponding inverse-projection consumer. If this is an auxiliary camera, inspect the three other known A31FF0 callers, then the DF3B50/+480 path.
3. Trace the **same owner's** visibility data. Follow `0070CA40` and `00C6DAD0` consumers of DF53E0 corners/bounds to the actual plane/AABB/sphere reject and draw-list insertion. Prove the branch which excludes an object, separating frustum rejection from occlusion/LOD. Shared offsets alone do not establish shared object identity.

Static completion requires the connected owner-to-projection-to-visibility chain, verified units and ABI. Live correlation is still required to select the actual gameplay camera.

## Minimal next MAIN experiment and FOV invariant

Implement one opt-in, bounded **same-frame capture**, with native patching disabled. For each eye, retain the exact D3D9 RT0 source used for that eye's D3D11 pass and dump the final eye cache before swapchain copy. Eye 0's source must be saved before replay can overwrite it. Capture only one requested loaded gameplay frame; use existing capture boundaries and avoid new frame scheduling.

Write one sidecar with engine/record/replay/present/XR frame IDs, eye, RT identity/format/dimensions, viewports, actual source view/projection after all matrix edits, native Px/Py, per-eye GPU scales/crop, final XrFovf radians and layer poses, XR eye separation, baseline units, color-space state, effective INI/build/EXE hashes and capture success. Reject mixed frames or missing metadata. Compare each eye against its own source with Luna's ray and bilinear baselines; this is the minimum evidence needed before changing sharpness.

**Native-wide implementation gate:** in `src/d3d9.cpp::UpdateSubmissionFovFromMatrices`, `g_engineFovFactor>1` currently resets global widening but does not unconditionally reset per-eye widening/cover values already >1. Calling a setter or toggling the factor cannot certify GPU widening removal. Recompute all per-eye values from the newly observed projection for the same frame; do not reuse old factors or apply the full old widening again. Preserve `P_eff=P_native*residualScale/crop` and derive submitted tangent bounds from the actual rendered rays. Keep symmetric metadata for symmetric rendering; asymmetric runtime recommendations alone cannot change it. Verify projection consumers, inverse projection and CPU coverage together. Existing `TryApplyEngineFovOverride` can reach `InstallGetFovProbe` despite the startup gate; `StartupMode=2` alone is not proof that native patching is off.

## Required live gates (both BLOCKED now)

**One controlled Steam run, after the executable/Bitdefender issue is resolved outside this review:** re-audit installed EXE identity and patch-site bytes; preserve DLL/INI hashes and record Steam/overlay/runtime state. Use a reviewed diagnostic build with all native patch installation paths demonstrably disabled, unchanged SubmitMode=4 and a fixed save/pose. Capture menu-to-loaded-gameplay ownership and the single matched RT0/eye pair above; confirm both eyes and frame IDs, baseline draw counts, no capture failure, and no new crash. Native-full-FOV remains NOT RUN. An observe-only camera/visibility witness may be added only after its ABI/access path is established; missing ownership remains BLOCKED. Do not force a widened run merely to fill the checklist.

**One PSVR2 validation session after that run passes:** user checks stationary fusion, each eye's detail separately, binocular near/far depth and peripheral objects in the fixed scene. Log real XR eye separation and an independent metric landmark; preserve FOV/crop/pose across comparisons. Report each symptom separately as PASS/FAIL/NOT RUN. Moving-head alignment is a separate gate, not inferred from stationary fusion. Native-wide acceptance later requires a fixed-pose baseline/wide comparison showing newly covered peripheral objects survive CPU visibility, both eye projections agree with submitted rays, and no doubled widening, lighting/depth reconstruction or stereo regression.

## Verification and limits of this review

- Re-ran Gemini's 10 tests (including its available historical private-trace regression): PASS; independent VS/PS fixture reproduced contamination. The cross-pass test validates the wrong contract.
- Re-ran Luna Python self-test and compiled/executed its x86 C++ production-helper test: PASS. The first isolated compile lacked a copied base header; after supplying unchanged `stereo_math.h`, it passed.
- Re-ran Sol's six parser tests: PASS. No fresh Ghidra investigation or binary execution.
- Branch path comparison and production-source review: verified. Native receiver identity, CPU visibility ownership, blur cause, metric scale and native full-FOV behavior: unproven.
- Historical stationary fusion belongs to the base; no new Steam or headset test was performed. Bitdefender quarantine/Steam executable failure remains the live-testing blocker.
