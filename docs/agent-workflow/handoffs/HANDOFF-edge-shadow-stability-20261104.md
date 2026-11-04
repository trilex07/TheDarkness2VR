# Handoff: native outline coverage and scoped shadow reconstruction

**Date**: 2026-11-04
**Task ID**: edge-shadow-stability-20261104
**Role**: MAIN
**Agent**: Codex
**Branch**: codex/edge-shadow-stability-20261104
**Worktree**: C:\Users\Trilex\Darkness2VR\.main-camera-shake
**Baseline**: accepted 324a2278 DLL95BB1C37, integrated main b6daa63f.
**Integration commit**: pending user headset acceptance; do not merge as solved.

## Goal and status

Reduce edge/hand shimmer and intermittent shadows at local lights and the
post-villa cutscene. **Ready for bounded candidate review; full task remains
unverified.** User says most shadows already look good. Preserve those paths.

## Context loaded

Root AGENTS, bootstrap, BASELINE, WORKFLOW, FAILED-APPROACHES and prior rendering
handoffs; skills game-automation, shader-programming, computer-use, VR wizard.

## Implemented

- Exact E345B150 outline PS,379 DWORDs, SHA256
  6b7fc867061f6bbe0d204e8ff4231bce103106572e569ba6ce653e54e6df9cec.
  Four native coverage evaluations at half-pixel offsets from DSX/DSY; retains
  original colour/depth/normal thresholds. No full-colour blur/history. Dedicated
  r16..28/c223. Diagnostic OutlineCoverageExperiment defaults OFF.
- Exact C4D1FB10 shadow PS,568 DWORDs, SHA256
  7fb0cc4d119b3f93336344f4d44bb3907d79465371fa3ef5d67c4062734bb751.
  Word234 originally `mul r0.xy,r0,r1.x`: depth-scaled NDC conflicts with the
  homogeneous geometric-head inverse. Runtime c223 selector:0 retains native
  tuple,1 uses NDC; r14/r15 unused by the exact native shader. Full-SHA gate.
- ShadowClipScope at all four real draw wrappers selects1 only for geometric
  head,mode0,projection compensation,default lighting profile(-1/0),inverse c27.
  Other contexts use native0. Restores actual c223, invalidates material cache.
  Shader metadata marks replacement only if driver creation succeeded. Defaults
  remain OFF; this is not a global fix for every shadow shader.
- Diagnostic one-frame shadow capture now records pre-draw target and blend
  contract. Actual state [15,1,9,1,1,0,2,2,1] means source*destination. Recover mask
  as after/before using a nonzero channel. Old RGB-only scores were INVALID.
- audit_shadow_temporal.py requires validated before/after/depth hashes, same
  target, camera, projection and light c0/c1/c16..19. Reprojects depth-consistent
  shadow-contribution pixels. Rejects old captures missing the blend contract.
- Frozen installer modes OutlineExperiment and EdgeShadowExperiment added;
  default Combined keeps experiments OFF and all accepted functionality.

## Evidence and limits

Private root: test-output/edge-shadow-20261104 (never commit game dumps/saves).

- PASS GPU outline test:48 subpixel steps, temporal L1 native2788170 vs1639276,
  ratio0.5879; energy146368980 vs146140518(-0.16%). Flat RGB preserved. All379
  single-word mutations rejected. This is a fixture, NOT41% whole-game benefit.
- Full-size3345x3410 fixture cost native0.125ms/pass vs0.569ms/pass on last run;
  earlier0.125 vs0.515. Extra0.39-0.44ms/eye; not game FPS/HMD cost.
- PASS real shader driver creation in actual alley. Hands/tentacles/stereo
  present in captures. Actual headset shimmer acceptance NOT RUN.
- Shadow control OFF:40664-1808-19458/1809-28185,1054 depth-consistent samples,
  mean0.0472887,median0.0410,p90 0.0991,9.39% error>0.1.
- Scoped ON:47328-1813-8957/1814-11171,3015 samples,mean0.00903976,
  median0.00635,p90 0.0200,0.199% error>0.1. Same native camera/light and same
  effective head projections as control. Different contributing sample sets;
  bounded one-consumer comparison, not universal shadow visual acceptance.
- c223 [1,0,0,0] observed in actual consuming draw. Final shader creation S_OK.
- PASS native selector GPU prefix: bit-exact RG pixels to native; corrected
  selector changes4096 observed channels. This tests reconstruction, not all
  shadow taps. Full replacement also accepted by real D3D9Ex driver.
- PASS actual-source scope test: live/both eyes, native/cinema branch, unsupported
  settings, unknown shader, failed getter, exact restore, all four wrappers.
- PASS actual-source pixel metadata/concurrency and shadow draw-wrapper tests;
  updated their stale fixtures with current dependencies.
- PASS diagnostic and production x86 builds. No resolution/NIS/AF/FXAA changes.
- NOT RUN: exact post-villa cutscene, all lamp families/omni D82658F7, headset,
  whole-game temporal score, controlled game FPS gain. Reflections remain open.

## Runtime/restoration

Owned emulator was closed; lab Restore PASS. Steam accepted DLL restored:
95BB1C3706D95CA0AE4A659BC497216E8C1EC757AED7CE692FA61B0BA122F286.
Diagnostic INI3A3FD69804D0E62A66E699652AB89C6BCD086CCB271CBE31A65B97F8071E6A6A.
Protected main INI22862511B4E5697F37413802D3B562DD388ED510C68D0E1CD745E629A5A79B08
retained (user SS125; do NOT replace with old SS100 frozen settings).
EXE/loader hashes unchanged. All4 EE.cfg/Editor.cfg/settings/CONTINUE.SAV
restored from saves.json with exact hashes. Receipt restored.json.
No active emulator/game should remain. Candidate is separate from accepted Steam.

## Failed/rejected approaches

- Quarter-pixel coverage ratio0.8661 failed20% fixture gate; half-pixel passed.
- Linear normal sampling ratio1.1838 worse; reverted.
- Comparing accumulated grayscale RGB selected irrelevant dark lighting pixels;
  old shadow-on-temporal.json is SUPERSEDED and invalid. Use before/after blend.
- Two-constant LRP and LRP with destination aliasing first source rejected by
  driver; final separate r14/r15 selector passes. Do not resurrect those forms.
- First scoped selector mistakenly admitted profile0 only; configured default
  is-1. Corrected gate and actual GPU readback now prove selector1.

## Next steps

1. Freeze production package with prepare-combined-scenes-candidate.ps1,
   ExpectedInstalledDll95BB1C37...; use EdgeShadowExperiment for combined trial.
2. Actual post-villa/cinema shadow and headset edge/hands comparison remain
   necessary before accepting/integrating. Preserve exact rollback/current SS.
3. If remaining local lights differ, identify their actual consumer; do not
   extend C4 fix to omni/atlas paths by guesses. No shader/AA FPS gain claim.
