# Baseline and current MAIN candidate

Recorded by MAIN on 2026-10-31. This file is a provenance snapshot, not a live
Steam check or permission to deploy. Do not assume Git main is installed.

## Accepted rollback baseline (retained, not currently installed)
- **Accepted DLL SHA256**: 3FDEB05CD94D6FC4E14C6C446A2859DA5F9A9E832AF5348D4602E4A7EF03C3EA
- **Accepted source commit**: 4a26aabdce15903eb5efe48bd707d47e200175e5
- **Accepted rollback**: %LOCALAPPDATA%/Darkness2VR/packages/nvidia-warmup-73a461b0-20261030/rollback

The retained rollback manifest verifies the DLL, all four INIs and four shader/
warmup caches. Restore the complete matching package, not only a DLL with an
incompatible newer manifest. Prior ConsumedRecordCopy validation preserved
stereo/hands/tracking. This does not certify newer experimental switches.

## Current installed MAIN candidate: X double tap weapon toggle

Authorized MAIN deployment completed 2026-10-31 20:25 Europe/Warsaw.
- **Current DLL SHA256**: F5046F99AFBC266E68404FA743603522964EDF47DE9E0D1E5C44116EBF25D367 (protected-native)
- **Source**: side/unarmed-trigger-dpad-on-installed 5b7b424f
- **Behaviour**: DoubleTapX=TOGGLE: right-hand firearm two-handed -> DPAD_LEFT (back to one-handed), one-handed -> DPAD_RIGHT (two-handed), empty main hand -> DPAD_DOWN, unknown -> alternate. Empty-hand trigger mapping OFF (UnarmedTriggerDpad default 0).
- **Evidence**: headset 20:21 confirmed DPAD_RIGHT selects the two-handed weapon and DPAD_LEFT returns to the uzi.
- **INI change**: Steam Darkness2VR.ini DoubleTapX DPAD_RIGHT -> TOGGLE (+ comment). No other key edited.
- **Package**: C:/Users/Trilex/AppData/Local/Darkness2VR/packages/unarmed-toggle-20261031T202547062 (rollback restores E7B6572A set).
- **Validation**: MSVC x86 build PASS (after renaming a lambda that collided with the Windows `small` macro), host tests PASS, profile gate PASS; headset NOT RUN.

## Previous MAIN candidate: configurable X double tap (DoubleTapX) (superseded)

Authorized MAIN deployment completed 2026-10-31 20:12 Europe/Warsaw.
- **Current DLL SHA256**: E7B6572AF8AF28BD7FBA4448E00BDE2C6A5F66C3079DFBB40796306791369B72 (protected-native)
- **Source**: side/unarmed-trigger-dpad-on-installed b45c82b4
- **Change**: [Input] DoubleTapX (read live on each X press) selects the double-tap button; default DPAD_RIGHT.
- **INI change**: Steam Darkness2VR.ini gained the DoubleTapX=DPAD_RIGHT key plus two comment lines under [Input]; nothing else edited. Local AppData INI untouched.
- **Package**: C:/Users/Trilex/AppData/Local/Darkness2VR/packages/unarmed-doubletapx-20261031T201250148 (rollback restores 3DDC8B49 set, including the pre-edit INI).
- **Validation**: MSVC x86 build PASS, host tests PASS, profile gate PASS; headset NOT RUN.

## Previous MAIN candidate: X double tap + unarmed trigger D-pad (superseded)

Authorized MAIN deployment completed 2026-10-31 19:54 Europe/Warsaw (DLL only, INIs untouched).
- **Current DLL SHA256**: 3DDC8B499AA180D167CC20F22710293D96BA47C6A8F40A971CAAE420C5141FED (protected-native)
- **Source**: side/unarmed-trigger-dpad-on-installed d3d77785
- **Change vs 1D24CC77**: double tap X (PSVR2 left Square) within 350 ms -> empty main hand DPAD_DOWN, otherwise DPAD_RIGHT; hold X untouched (heart devour); native hand state held 1 s across stale samples. Empty-hand trigger mapping retained.
- **Reason**: runtime log 19:46 showed DPAD_RIGHT reaching XInput (Btns=0x0008) with main=armed off=empty, but no in-game dual wield; user requested an explicit button. Whether stock D-pad right dual-wields with the current inventory is unverified.
- **Package**: C:/Users/Trilex/AppData/Local/Darkness2VR/packages/unarmed-doubletap-20261031T195455452 (rollback/Restore.ps1 restores 1D24CC77 set).
- **Validation**: MSVC x86 build PASS, host tests PASS, profile gate PASS; headset NOT RUN. Log tags: `[DoubleTapDpad]`, `[UnarmedTriggerDpad]`.

## Previous MAIN candidate: unarmed trigger D-pad on installed shouldrender-fix build (superseded)

Authorized MAIN deployment completed 2026-10-31 19:39 Europe/Warsaw (DLL only, INIs untouched).
- **Current DLL SHA256**: 1D24CC777D4F9001A0437E71696BC84E77A96351BEEFB0D40D14FB253967E442 (protected-native)
- **Source**: side/unarmed-trigger-dpad-on-installed 0fd55383, on side/shouldrender-fix-snapshot 2ae798d3
- **Replaced DLL**: 5743F3F471B6713CE4055FF207EB17EE6AC7B96AC4845C579B17F15CFA44CF6E, installed 18:49 from the uncommitted .side-shouldrender-fix tree (HiddenSubmit, window placement, S_PRESENT_MODE_CHANGED, AdaptiveFPS missing-key default AUTO). That tree is now recorded as 2ae798d3. Its installation had no receipt; the half-rate DLL it replaced is d3d9.dll.off-hiddensubmit-20261031-181925 in the game folder.
- **AdaptiveFPS**: AUTO (installed INIs carry no key; code default AUTO). User confirmed AUTO 2026-10-31 19:38.
- **New feature**: [Input] UnarmedTriggerDpad (default 1): L trigger + empty off hand -> DPAD_RIGHT (dual wield), R trigger + empty main hand -> DPAD_DOWN (draw). Unknown native hand state passes triggers through.
- **Package**: C:/Users/Trilex/AppData/Local/Darkness2VR/packages/unarmed-trigger-dpad-20261031T193931147 (deployment.json, candidate DLL/PDB/MAP, rollback/manifest.json + Restore.ps1, all nine DLL/INI/cache files hash-verified).
- **Validation**: MSVC x86 build PASS, host test unarmed_trigger_dpad PASS, profile gate PASS. Game not launched; headset validation NOT RUN. Check d3d9.log for `[UnarmedTriggerDpad] native hands`.

## Previous MAIN candidate: adaptive half-rate (superseded)

Authorized MAIN deployment completed 2026-10-31 16:20 Europe/Warsaw.
- **Current DLL SHA256**: 4B50EAF295135D8BBB06B039D91FA739D114266A189C3572D95D0890DF1693C3
- **Current source commit**: 346fac9327493ac6fffc2ebd2bdade07ad52e824
- **Current Steam INI SHA256**: 2DF452F069FFCF4D2A52BF6EE6F040436991F4FAC87A71E9B5EC136D926F1F86
- **Current local INI SHA256**: 574657E14011831565CBE8C3C2F8B4C36F403F854FB45E47891B01D88E9E408A
- **Deployment package**: C:/Users/Trilex/AppData/Local/Darkness2VR/packages/adaptive-half-rate-20261031T162023837
- **Rollback**: package rollback/Restore.ps1; all nine predeployment DLL/INI/cache files independently hash-verified.
- **Receipt/symbols**: package deployment.json and candidate/ (DLL/PDB/MAP).
- **Changes**: protected-native DLL plus Render/AdaptiveFPS=OFF in Steam/local profiles after user rejection (2026-10-31). All other profile contracts and four caches preserved. Quarantined OverlapNativePresent=0 and ReuseSharedCaptureColor=0 retained.
- **Validation**: both x86 builds and affected offline policy/deadline/VR settings/StateBlock lifetime checks passed. One adaptive host test compiled but execution blocked by Windows sharing error. EXE SHA256 remains D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1.
- **Runtime status**: user reported the feature does not work and requested disabling it. Runtime log showed HALF at 120 Hz with approximately 58 app FPS, which does not establish compositor synthesis. AdaptiveFPS is now OFF in both primary profiles. DLL restored and hash-verified after temporary native-menu investigation. HUD remains visible; menu input automation did not respond, so HUD OFF is NOT completed. Disable receipt/backups: C:/Users/Trilex/AppData/Local/Darkness2VR/packages/disable-adaptive-20261031T165924794. Accepted rollback baseline above remains unchanged.
- **Handoff**: handoffs/HANDOFF-adaptive-half-rate.md. Task branch codex/adaptive-half-rate; previous integration branch remains codex/main-single-owner-d3d9-validation.

## Previous MAIN candidate: four-branch performance (retained by current rollback)

Updated 2026-10-31 after authorized four-branch deployment. Both x86 builds and
affected offline regressions PASS; this new combined candidate is NOT headset-accepted.
- **Current DLL SHA256**: BFF8B0A9F5BE83C511762E66C78C5903BD9FB3A48FD3D2F81BF15C035FBD1CFA
- **Current source commit**: 7159787160cf1702836ebd08de4594805421d460
- **Current Steam INI SHA256**: C252004214079D174E89D32A1D2F3EF985C109EB0B0FCAC76B103F2A2AF8E692
- **Current local INI SHA256**: 6083B9E7A3BEE21464A884FD73AD13657A3C89AB2260CCEBDCEE41197E722049
- **Current rollback**: %LOCALAPPDATA%/Darkness2VR/packages/four-branch-performance-20261031/rollback
- **Receipt and matching symbols**: same package, deployment.json and candidate/
- **Runtime feedback**: intermittent right-eye artifacts during gameplay. Combined candidate NOT visually accepted; cause not established. Log/profile/cache preserved in four-branch-performance-20261031/runtime-20261031T155300 and quarantine-20261031T155425.
- **Current new switches**: OverlapNativePresent=0 and ReuseSharedCaptureColor=0 quarantined after feedback; PipelineVariantLearning=1 retained. DLL unchanged; no new launch or FPS claim.
- **Preserved**: Mode4, ConsumedRecordCopy=1, PipelineWarmup=1, DirectRecordCopy=0, ReplayBufferVersions=0, diagnostic NativeCameraCover=0. Game not launched.
- **Task handoff**: handoffs/HANDOFF-four-branch-performance.md

### Previous installed candidate (retained by current rollback)

- **Installed DLL SHA256**: 34641A8981444EA05723E6F5F3D669099F6FECD3ABC1142ED1C686C6E42EC232
- **Installed INI SHA256**: 764EEADF2E5FB21E4F1A95B3884F39B8E75D3E94765CC10F74C9BCE5D5B8DF0B
- **Installed local INI SHA256**: FE242875007DA0BFDCE05B99A15256A22256550609A934CB4BA2BE18E0D17020
- **Deployed DLL source commit**: f24ced92f01823f070e1a2b9361d72b920ed6b92
- **Verified source commit**: f24ced92f01823f070e1a2b9361d72b920ed6b92 (exact frozen diagnostic DLL/PDB/MAP build)
- **Integration branch**: codex/main-single-owner-d3d9-validation
- **Latest MAIN runtime evidence commit**: eab97b35
- **Acceptance status**: StereoTransferWorker=4 retained provisionally; not unconditional acceptance or stable 90 FPS
- **Previous rollback**: %LOCALAPPDATA%/Darkness2VR/packages/single-owner-d3d9-20261031/rollback

Current settings include ConsumedRecordCopy=1, StartupShaderPool=1,
PipelineWarmup=1, StereoTransferWorker=4, NativeViewBoundary=3,
NativeCameraCover=0, DirectRecordCopy=0, ReplayBufferVersions=0. Source defaults
are not deployed settings. Read the current handoff/evidence before changing any.

One paired authorized mode-1/mode-4 test is already complete. Same diagnostic
DLL, identical settings apart from mode, restored identical startup caches,
warmup 1377/1377, zero PresentMon losses, real device flags 0x54 vs 0x50.
Application interval mean 14.73 -> 13.73 ms; p95 18.09 -> 15.50 ms;
p99 19.49 -> 16.96 ms. Qualified user feedback: visuals appear correct and the
light collector no longer noticeably costs ~20 FPS. Both x86 builds and 19
regression suites passed. No objective headset latency measurement.

## Current unresolved performance work

A 58.98 ms worst interval remains; the enclosing D3D9 producer-fence window
reached 42.656 ms. Its exact driver dependency is unproven; do not identify it
with the earlier 15.7 ms NVIDIA event or blame transfer ownership without proof.
NativeSubmissionFilter/NativeBindingFilter fail closed equally in both runs
(enabled=0, denied=0x12, skipped=0). No speculative cache/admission bypass.

Evidence: [MAIN comparison](../main-single-owner-d3d9-validation.md),
[retained measurements](../validation/single-owner-d3d9-20261031.json).
Continue [the performance handoff](handoffs/HANDOFF-single-owner-d3d9.md).
Do not redo integration or repeat the completed paired capture by default.

## Update rule

MAIN updates this snapshot only from verified build/deployment identities and
explicitly qualified runtime evidence. Documentation commits are not DLL source
commits. SIDE may cite this file but never update accepted/deployed status,
install files, launch Steam or perform in-game tests. No build/install/runtime
smoke test is needed for documentation-only changes.
