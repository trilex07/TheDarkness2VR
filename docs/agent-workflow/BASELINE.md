# Baseline and current MAIN candidate

## Installed read-only selected shake-owner trace (2026-11-01)

- **Installed DLL SHA256**: `7414984FF998FABDF154AEB5EE443D75805378DDB2359F7EC6F20E83E8A6FA55`, source `6eb59a04` on `codex/gaze-nis-main-integration`. Diagnostic INI SHA256 `30EDBFD53636856E57312375983749D9FB5D8ABB8548E43EE837F0D4782988FD` retains `ShotgunCandidate=1` and `CameraShakeTrace=1`, with no suppression request. Stock EXE unchanged.
- **Change**: read-only selected avatar vtable/field trace added to existing camera/hand trace. No native image patch or gameplay transform change. Headset owner sample NOT RUN at installation.
- **Subsequent read-only result**: selected avatar table is `01001630`, not `00FDF170`; a one-shot live scan found two avatar instances sharing a `00FDF170` object through `avatar+0x5E0`. Idle noise outputs were zero. No shake suppression is active or physically accepted.
- **Rollback**: `%LOCALAPPDATA%/Darkness2VR/packages/camera-shake-owner-trace-20261101T181538632/rollback/Restore.ps1`. Independent audit: 9/9 backups, 8/8 unchanged protected targets, exact DLL/INI/stock EXE hashes and no running game. The agent did not launch the game.

## Current after procedural shake trial rollback (2026-11-01)

- **Current installed DLL SHA256**: `EF58E6F6BEDEADF929F9021E562ED33C5D9A806E341CDEB950775D7C1A878567`; diagnostic INI SHA256 `30EDBFD53636856E57312375983749D9FB5D8ABB8548E43EE837F0D4782988FD` retains `ShotgunCandidate=1` and read-only `CameraShakeTrace=1`. Stock EXE unchanged. User reports the shotgun now works; left-hand grip, exact aim and both-eye details were not separately graded.
- **Trial result**: camera still shook; the EXE call-site intervention did not activate because `BlockNativeImagePatches=1` and `RestoreTrackedGameplay=0`. Startup log has `requested=1` but no `active=1`. This is a failed admission, not an A/B result for the generator.
- **Rollback**: `%LOCALAPPDATA%/Darkness2VR/packages/camera-shake-suppress-20261101T180426583/rollback/Restore.ps1` restored all nine protected targets after the game closed. Independent audit: 9/9 current hashes match the pretrial manifest, previous DLL/INI exact, no game process.

## Installed opt-in procedural camera-shake trial (2026-11-01)

- **Installed DLL SHA256**: `CC53C2E4D0EFEFAA3E713B0E3404148F07DF20BE61363AFEFC0D3D40B6992EB1`, source `d1ed1aa9` on `codex/gaze-nis-main-integration`. Diagnostic INI SHA256 `7E1160EF7C634EFFBD356AE21CCF7EB44F28C0218E633A062EDD2AF19D179687` enables `CameraShakeSuppress=1` while retaining `CameraShakeTrace=1` and `ShotgunCandidate=1`. Stock EXE unchanged.
- **Change**: the original procedural generator still runs, but its output is zeroed at four pinned first-person update calls. The other two generator callers, native animation, controller IK, gun and camera matrices are not directly changed. This is a physical test candidate; shake, hand/gun stability and both-eye results are NOT RUN.
- **Rollback**: `%LOCALAPPDATA%/Darkness2VR/packages/camera-shake-suppress-20261101T180426583/rollback/Restore.ps1`. Independent audit verified 9/9 backups, 7/7 unchanged protected targets, exact DLL/diagnostic INI/stock EXE hashes and no running game. The agent did not launch the game.

## Installed read-only camera, native hand and weapon-grip trace (2026-11-01)

- **Installed DLL SHA256**: `EF58E6F6BEDEADF929F9021E562ED33C5D9A806E341CDEB950775D7C1A878567`, source `f3fad557` on `codex/gaze-nis-main-integration`. Game diagnostic INI SHA256 `30EDBFD53636856E57312375983749D9FB5D8ABB8548E43EE837F0D4782988FD` retains `CameraShakeTrace=1` and `ShotgunCandidate=1`; stock EXE unchanged.
- **Change**: same-frame native left/right wrist and weapon-grip bones are logged before controller IK alongside selected camera/root for the first 30 gameplay seconds. No transform is changed. Camera shake suppression remains NOT RUN; headset sample pending.
- **Rollback**: `%LOCALAPPDATA%/Darkness2VR/packages/camera-shake-hands-trace-20261101T175229197/rollback/Restore.ps1`. Independent audit verified 9/9 backups, 8/8 unchanged protected targets, exact DLL/diagnostic INI/EXE hashes, and no running game. The user was asked to test sprint and heart consumption; the agent did not launch the game.

Recorded by MAIN through 2026-11-01. This file is a provenance snapshot, not a live
Steam check or permission to deploy. Do not assume Git main is installed.

## Installed read-only camera shake trace (2026-11-01)

- **Installed DLL SHA256**: `B92C42F4A84161BFF7F4597B1C412C7B6B784764DB9798F95393A10637A2301E`, source `8523e2ca` on `codex/gaze-nis-main-integration`. Game diagnostic INI SHA256 `30EDBFD53636856E57312375983749D9FB5D8ABB8548E43EE837F0D4782988FD` enables `CameraShakeTrace=1` and retains `ShotgunCandidate=1`; stock EXE hash unchanged.
- **Change**: selected player camera and avatar root transforms are read at 20 Hz for the first 30 gameplay seconds. No camera, hand or gun transform is changed by this trace. Camera shake suppression and headset results remain NOT RUN.
- **Rollback**: `%LOCALAPPDATA%/Darkness2VR/packages/camera-shake-trace-20261101T173845860/rollback/Restore.ps1`. Independent audit verified nine backups, seven unchanged protected targets, exact DLL and diagnostic INI hashes, stock EXE and no running game.

## Installed gated shotgun aim/support trial (2026-11-01 15:10 Europe/Warsaw)

- **Installed DLL SHA256**: `00F659DF51A7D3ED43084A837D190F2E288BB05861A464E31721FA32DDA99139`, source `a4188551` on `codex/gaze-nis-main-integration`. Game diagnostic INI SHA256 `2B6D94F228A156ACAFCF9043FE44E65DF64AAB1706DAF5CEC8540D3F8319DA49` enables `ShotgunCandidate=1`; stock EXE hash remains `D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1`.
- **Change**: exact shotgun action table routes its final shot through the protected controller bridge; the model changes only after that action has accepted a shot. A nearby left squeeze supports the left hand at the shotgun's native animated fore-end and consumes the demon-arm grip until release. This is a physical test candidate; aim, hit and support results are NOT RUN.
- **Rollback**: `%LOCALAPPDATA%/Darkness2VR/packages/shotgun-aim-support-20261101T151042620/rollback/Restore.ps1`. Independent post-install audit verified all nine backups and all seven other unchanged protected targets, exact DLL and diagnostic INI hashes, stock EXE hash, and no game process. Camera shake suppression is not included.

## Installed shotgun pose observer after failed model-only trial rollback (2026-11-01)

- **Installed DLL SHA256**: `867E3DE9E96920836F9B3CE761AF53875029F6A89A26EB43AFC6E39118C3EE0C`, source `1e0925e9` on `codex/gaze-nis-main-integration`. Stock EXE SHA256 `D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1` unchanged.
- **Protected rollback**: `%LOCALAPPDATA%/Darkness2VR/packages/shotgun-model-trial-20261101T141729420/rollback/Restore.ps1` restored all nine protected targets, independently hash-checked 9/9 after the user closed the game.
- **Gate**: headset model-only shotgun trial FAIL (aimed model, missed shots, right fingers short of trigger). The rejected DLL `5389DF98690B1A4D67E4585329E1DD525DE257C7F1E95E0B187A6DA5B7101BDF` is no longer installed. The observer contains no shotgun gameplay adjustment; two-hand grip and shake suppression are still pending. See `HANDOFF-shotgun-twohand-shake-20261101.md`.

## Installed held-finisher A candidate (2026-11-01 13:42 Europe/Warsaw)

- **Installed DLL SHA256**: `DDDEF1CD8FB135A7D89E6C515786D809351926914FC1FF1C195ADF1F27B6F5BF`, source `c9b86d0c` on `codex/gaze-nis-main-integration`; stock EXE SHA256 unchanged. The user-confirmed four small tentacle correction is included.
- **Change**: physical A now routes to the stock XInput A action for a fresh native alternate held target, including cases where `demonHold` remains 1. Ordinary gameplay's configurable A-as-X reload and menu confirmation are retained. `[PhysicalA]` and `[FinisherRoute]` logs distinguish proxy choice from the button returned to the engine.
- **Rollback**: `%LOCALAPPDATA%/Darkness2VR/packages/held-finisher-a-20261101T134251667/rollback/Restore.ps1`; all nine backup hashes and nine installed target hashes independently verified, with only the DLL changed. Previous DLL `EFDCCFB0B40F37F716014DF836F5BE127121177B5C88EABC3E3714B828375B9C` retained. Game closed and not launched by the agent.
- **Gate**: protected-native x86 build and profile PASS, dual weapon wheel 137 checks PASS; **held-enemy finisher headset result: NOT RUN**. See `HANDOFF-held-finisher-a-20261101.md`.

## Installed four small tentacles head-attachment trial (2026-11-01 13:32 Europe/Warsaw)

- **Installed DLL SHA256**: `EFDCCFB0B40F37F716014DF836F5BE127121177B5C88EABC3E3714B828375B9C`, source `88ea3095` on `codex/gaze-nis-main-integration`. Stock EXE SHA256 unchanged: `D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1`.
- **Change**: the existing `HeadAttachedTentacles=1` render scope now admits the separately captured four smaller headless player tentacle draws by full VB/IB/shader/matrix signature. Fresh native Empty/no-held-target, stereo, non-shadow and head-geometry gates remain. Both game and local INIs and all caches were left byte-identical; NIS 2 and LOD 1.25 remain active, DLSS stays OFF.
- **Rollback**: `%LOCALAPPDATA%/Darkness2VR/packages/small-tentacles-head-20261101T133218114/rollback/Restore.ps1`; nine backup hashes and all nine post-install protected targets independently verified. The prior DLL `CC7DD6C7B048158255D607649841BDE6305D284414CAD790A4CE99B14A941248` is retained in the rollback. The DLSS host directory remains absent. Game was closed and not launched by the agent.
- **Offline gate**: actual-source scope fixture 27,099 checks PASS; protected-native x86 build and profile gate PASS. **Headset result**: user reports the four small tentacles now follow the head correctly; installed runtime log confirms four admitted small-mesh draws. Both-eye/held-target specifics were not separately reported. User also reported the separate held-enemy finisher on physical A fails; see `HANDOFF-held-finisher-a-20261101.md`.

## DLAA integration trial rejected and fully restored (2026-11-01)

- **Current installed DLL SHA256**: `CC7DD6C7B048158255D607649841BDE6305D284414CAD790A4CE99B14A941248`, source `b9b1d190`. Stock EXE SHA256 `D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1`. The pretrial nine protected targets were restored and independently verified 9/9; the previously absent `d2vr_dlss/` host directory is absent again. Game was closed and not relaunched by the agent.
- **Current game profile**: `EngineLODScale=1.25`, `FoveatedCopyBase=1`, `NisMode=2`, `NisSharpness=0.80`, `NativeViewBoundary=3`, with no `DlssMode` override (default OFF). Restore package: `%LOCALAPPDATA%/Darkness2VR/packages/dlss45-dlaa-20261101T131236586/rollback/Restore.ps1`.
- **Headset result**: user reported a very blurry image and FPS falling to approximately 50. This candidate failed the headset quality/performance gate. Runtime log independently showed NGX initialized, DLAA features for both eyes at 2676x2728, and 4500/4500 frames per eye processed with zero refusals. At that point NGX GPU evaluate averages were 3.67 ms left and 2.95 ms right. The log does not independently establish the exact 50 FPS or isolate all frame-time causes. Missing native per-eye depth/jitter and rotation-only motion are plausible blur causes, not proven root causes.

## Historical DLAA integration trial installation (2026-11-01 13:12 Europe/Warsaw)

- **Installed DLL SHA256**: `398B262A36582E136BBCF52C061681D3A7263FC111BFE1A51C47913A29ED1F42`, source `c9bb4057` on `codex/gaze-nis-main-integration` (SIDE DLSS source `8f67a69d` integrated and hardened). Stock EXE SHA256 `D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1` unchanged.
- **Active game profile**: `DlssMode=1` (DLAA), `DlssPreset=11` (K), `DlssBackend=0` (NGX), `DlssObjectMotion=0`, `DlssBiasMask=0`, `NisMode=0`. Existing `EngineLODScale=1.25` and `NativeViewBoundary=3` retained. Repo default remains `DlssMode=0`.
- **Host**: seven files installed to `d2vr_dlss/`, including `d2vr_dlss_host64.exe` and local `nvngx_dlss.dll` v310.9.1.0. All seven installed hashes verified; the directory was absent before this trial.
- **Rollback**: `%LOCALAPPDATA%/Darkness2VR/packages/dlss45-dlaa-20261101T131236586/rollback/Restore.ps1`; 9/9 protected backup hashes verified. Host manifest records prior absent files for removal on restore. Package source receipt confirms `c9bb405711751357347821e93ad13116612e1651`.
- **Offline checks**: protected-native x86 build and profile gate PASS, x64 host build PASS, standalone NGX/FSR IPC/GPU suite 30/30 PASS. DLAA at 2676x2728 in the synthetic suite cost roughly 2.7-2.8 ms GPU and 3.5 ms present-thread CPU per eye. These are not in-game FPS or engine CPU measurements.
- **Headset gate at installation**: NOT RUN. Subsequent headset result is recorded above. Native per-eye depth and projection jitter were not connected; the trial used a constant depth plane and rotation-only camera motion.

## Installed R2 post-load candidate with LOD 1.25 (2026-11-01 12:15 Europe/Warsaw)

- **Installed DLL SHA256**: `CC7DD6C7B048158255D607649841BDE6305D284414CAD790A4CE99B14A941248`, source `b9b1d190` on `codex/gaze-nis-main-integration`. Stock EXE unchanged.
- **Profile**: `EngineLODScale=1.25`, `NisMode=2`, `NisSharpness=0.80`, `FoveatedCopyBase=1`; no explicit fovea overrides. The rejected LOD 2.0/NIS trial remains off.
- **R2 fix**: menu confirmation now checks two equal snapshots through the pinned `RegionMgr -> GameRules` pause graph, once per trigger press. Reset on world/manager changes. Post-death/load headset test and pause-menu confirmation are pending.
- **Rollback**: `%LOCALAPPDATA%/Darkness2VR/packages/r2-postload-lod125-20261101T121504038/rollback/Restore.ps1`; 9/9 backup hashes verified.

## LOD 2.0/NIS trial rejected and restored (2026-11-01)

- User perceived fewer frames, sometimes around 80 FPS, and R2 jumping after death/load. The combined trial does not isolate LOD from scene or NIS effects. Log showed periods of 77–80 XR layers/s; these are not native engine CPU milliseconds.
- Full nine-target rollback from `%LOCALAPPDATA%/Darkness2VR/packages/lod2-gaze-nis0-20261101T120551448/rollback/Restore.ps1` passed. Installed DLL returned to `999047FBE28943C29FE5C06B601B657B5122DC0B88915FC4AC18B65BB17C24F5`, game INI to `EngineLODScale=1.25` and `NisSharpness=0.80` with no explicit fovea overrides. Caches restored as a matching set.
- R2 post-load failure traced to reading pause fields from `RegionMgr` instead of the actual `GameRules` receiver. `HANDOFF-r2-postload-20261101.md` tracks the fix; headset verification pending.

## Installed LOD 2.0 and gaze NIS periphery trial (2026-11-01 12:05 Europe/Warsaw)

- **Installed DLL SHA256**: `A174A1199450BBE1C3D7933E81611EC0FFFA8165B3AFC54C651A4DB0EFCF5DD1`, source `bc8e57cc` on `codex/gaze-nis-main-integration`. Stock EXE unchanged.
- **Active game settings**: `EngineLODScale=2.0`, `FoveaPeriphery=0`, `FoveaSharpness=1.0`, `NisSharpness=0`; `NisMode=2` and `FoveatedCopyBase=1` retained. With valid gaze, copy-base keeps outside pixels unsharpened; this does not render a 2x resolution gaze region.
- **Rollback**: `%LOCALAPPDATA%/Darkness2VR/packages/lod2-gaze-nis0-20261101T120551448/rollback/Restore.ps1`; 9/9 backup hashes verified. Previous R2/LOD 1.25 candidate retained. Headset quality/FPS and R2 check pending.

## Installed R2 follow-up with native LODScale trial (2026-11-01 11:53 Europe/Warsaw)

- **Installed DLL SHA256**: `999047FBE28943C29FE5C06B601B657B5122DC0B88915FC4AC18B65BB17C24F5`, source `7313549d` on `codex/gaze-nis-main-integration`; stock EXE unchanged.
- **Change**: current native pause recheck before R2 can become menu A; `[R2Route]` logs one line per R2 press. This addresses a possible stale pause gate. Headset verification pending; root cause is not yet confirmed.
- **LOD**: `EngineLODScale=1.25` retained. Prior headset trial: user reported smoother play and correct image. Runtime logged `active=1`, `enabled=1`, `mode=-1`, `lowest=0`; CPU milliseconds saved are unmeasured.
- **Rollback**: `%LOCALAPPDATA%/Darkness2VR/packages/engine-lod-r2-20261101T115339035/rollback/Restore.ps1`; all nine backup hashes verified. Previous LOD candidate DLL `9AFA35F2A4B3B74EFA79FBF2C10BCEA296F6A159126B407E1783B8E84F57A358` retained in rollback.
- **R2 headset acceptance**: user reported R2 no longer jumps. Same run logged eight `[R2Route] menu=0 world=1 inputContext=1 stickY=0 buttonsBefore=0x0000` presses, so those presses stayed on the gameplay trigger path. No pause-menu R2 confirmation was observed in this log.

## Installed native LODScale trial (2026-11-01 11:33 Europe/Warsaw)

- **Installed DLL SHA256**: `9AFA35F2A4B3B74EFA79FBF2C10BCEA296F6A159126B407E1783B8E84F57A358`, source `97fd19cd` on `codex/gaze-nis-main-integration`. Stock EXE SHA256 unchanged: `D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1`.
- **Setting**: active game `EngineLODScale=1.25`, applied only in 3D gameplay. This is a reversible whole-scene engine LOD trial, not gaze/per-object LOD. No headset quality or CPU/FPS result yet.
- **Rollback**: `%LOCALAPPDATA%/Darkness2VR/packages/engine-lod-trial-20261101T113328024/rollback/Restore.ps1`; all nine backup hashes verified. Previously installed stable DLL was `E6F49A7E9E520D24502B9F5ABAFB4E0DAE6A9D7DDC0ACDE86486DAD4128DB1BD`. See `HANDOFF-gaze-lod-candidate-20261101.md`.

## LOD observation completed and stable DLL restored (2026-11-01)

- **Installed DLL SHA256**: `E6F49A7E9E520D24502B9F5ABAFB4E0DAE6A9D7DDC0ACDE86486DAD4128DB1BD`, restored stable gaze-dot/R2 source `70f4e28e`.
- **Restoration/rollback**: `%LOCALAPPDATA%/Darkness2VR/packages/gaze-lod-observer-revert-20261101T112155709/rollback/Restore.ps1`; all nine backup hashes verified; current caches retained. Active game INI no longer contains `GazeLodRuntimeObserver`; `GazeDot=0` retained.
- **LOD evidence**: hardware execution observer refused occupied DR state and made no quality change. Read-only live scan found one stock owner vtable `00FF8818`, a non-null `+274` renderer and scene-list references. Exact object classes, CPU savings and a safe gaze LOD adjustment remain unproven. Windows refused the attempted CPU.Verbose ETW profile (`0xC5585011`).
- **Integration**: research and diagnostic commits remain on `codex/gaze-nis-main-integration`; remote main remains the stable gaze-dot/R2 source. See `HANDOFF-gaze-lod-optimization-20261101.md`.

## Current installed MAIN candidate: gaze dot and R2 fix (2026-11-01)

- **Installed DLL SHA256**: `E6F49A7E9E520D24502B9F5ABAFB4E0DAE6A9D7DDC0ACDE86486DAD4128DB1BD` (protected-native x86).
- **Source**: `70f4e28e7fea44c97ff7a478cfb93b15d2318d04`; main promotion recorded in `HANDOFF-gaze-dot-r2-jump-20261101.md`.
- **Package/rollback**: `%LOCALAPPDATA%/Darkness2VR/packages/gaze-dot-r2-20261101T031753160/rollback/Restore.ps1` (nine targets; all backup hashes verified).
- **Change**: active game `GazeDot=0`; R2 maps to pad A only during pause or when no 3D world is visible. Active-world Flash cursor no longer makes R2 jump.
- **Offline evidence**: eye tracking 29/29 and dual weapon wheel 137/137; protected profile PASS; installed DLL and stock EXE hashes verified.
- **Headset acceptance**: pending user test of no gaze dot, R2 firing without jump, and pause-menu confirmation.

## Current user-approved MAIN integration (2026-11-01)

- **Installed DLL SHA256**: `70F43D5D414887BE33270160686073541D61E3A9CBBB05877C5B65FE8CEB5A7F` (native CPU submission candidate, protected-native x86).
- **Installed source**: `91ff126a`, with deployment receipt `fcd11456`; local and GitHub main promotion follows this headset test.
- **Current rollback**: `%LOCALAPPDATA%/Darkness2VR/packages/native-cpu-retain-20261101T030900484/rollback/Restore.ps1` protects DLL, four INIs and four caches.
- **CPU headset evidence**: NativeSubmission enabled=1 denied=0; 1,317,205 VS constant, 781,594 PS constant and 182,168 binding calls skipped in a cumulative session sample. Both TrueStereo eyes succeeded with overflow=0. User reported correct image/weapons and better FPS. Exact app/engine CPU milliseconds saved are unmeasured; current scene's XR cadence near 90/s is not native simulation FPS.
- **Previous installed gaze NIS DLL**: `916B9A5151CC25F694A0B967862F3CEFFCC21CE23E8C6D857BF654B164DB7FDF`, retained in the current rollback.
- **Source**: `a2fdaf58` on `codex/gaze-nis-main-integration`, merged with HUD/gaze evidence at `316135c`. This integrates the installed wheel, gaze, frame-cap, HUD and NIS source; the game DLL was built from the corresponding isolated candidate source.
- **Earlier gaze NIS rollback**: `%LOCALAPPDATA%/Darkness2VR/packages/gaze-nis-copy-base-20261101T021726069/rollback/Restore.ps1` backs up the DLL, four INIs and four caches.
- **Headset evidence**: NIS pair GPU time ~0.13 ms with valid gaze versus ~0.28–0.30 ms in the earlier foveated build. Both eyes took the fast path. Recent XR layers were near 90/s, but this is not native simulation FPS and does not establish an engine CPU gain. User requested promotion to main; visual quality feedback remains open.
- **Native engine LOD**: unchanged. The tested camera+2A4 owner reference was null; object classification and safe selector ownership remain unproven.

## Accepted rollback baseline (retained, not currently installed)
- **Accepted DLL SHA256**: 3FDEB05CD94D6FC4E14C6C446A2859DA5F9A9E832AF5348D4602E4A7EF03C3EA
- **Accepted source commit**: 4a26aabdce15903eb5efe48bd707d47e200175e5
- **Accepted rollback**: %LOCALAPPDATA%/Darkness2VR/packages/nvidia-warmup-73a461b0-20261030/rollback

The retained rollback manifest verifies the DLL, all four INIs and four shader/
warmup caches. Restore the complete matching package, not only a DLL with an
incompatible newer manifest. Prior ConsumedRecordCopy validation preserved
stereo/hands/tracking. This does not certify newer experimental switches.

## Current installed MAIN candidate: eye tracking v5 + weapon wheel v4 (2026-11-01 00:22 Europe/Warsaw)

User ran the launcher (build + host test + bridge build + preflight + install), game closed; no agent launch.
- Source: `989c727d`, branch `main-weapon-wheels` (includes v4 ammo counters `1f6ff105`).
- Installed DLL SHA256: `E5DEFFDF9C45977F2BD10BEAFCA2D4465863E13E0B1911C9675451DC3F6A2D1A` (protected-native).
- Installed bridge `%LOCALAPPDATA%/Darkness2VR/runtime/psvr2/Darkness2Psvr2Bridge.exe` SHA256 `639F14605BC1FD3D17625E1DE0920464DD11BFAFAAE3E66E7F159A4EEF14401F`
  (publishes Toolkit gaze); previous helper saved as `rollback/Darkness2Psvr2Bridge.exe.previous` (not restored by Restore.ps1).
- Package: `C:/Users/Trilex/AppData/Local/Darkness2VR/packages/eye-tracking-v5-20261101T002252644`; DLL rollback `rollback/Restore.ps1`.
- v4 headset: ammo counters confirmed counting shots/reloads (log). v5 gaze axes/rate PENDING headset check.

## Previous installed MAIN candidate: weapon wheel v3 (2026-10-31 23:33 Europe/Warsaw)

User ran build + preflight + install (game closed); no agent launch. A second launcher run
afterwards correctly refused at preflight ("Installed DLL changed") because v3 was already in place.
- Source: `52bcab50`, branch `main-weapon-wheels`.
- Installed DLL SHA256: `7F98281E3EB154593EEF927AC6A5E12293EF3B8AE10550548CB2254C14D0B8B2` (protected-native).
- Items single/dual/two-handed, learned closed-loop D-pad equip; A=reload only in gameplay; tentacle object hold no longer blocks wheel/flicks.
- Package: `C:/Users/Trilex/AppData/Local/Darkness2VR/packages/weapon-wheels-v3-20261031T233323096`; rollback `rollback/Restore.ps1`.
- Offline: dual_weapon_wheel MSVC /W4 /WX 129 PASS; protected build PASS. Headset acceptance PENDING.

## Previous installed MAIN candidate: weapon wheel v2 (2026-10-31 23:18 Europe/Warsaw)

User ran build, preflight and install with the game closed; no agent launch.
- Source: `e0109d8e`, branch `main-weapon-wheels`.
- Installed DLL SHA256: `859F5E3A96159AD5D147A1370A5F4C7A7777BEF5642557A406C866AD4052B870` (protected-native).
- Single wheel at the right hand, closed-loop D-pad equip; right stick Y never pitches (up JumpButton=A, down CrouchButton=R3); physical A = AButton=X (reload).
- Package: `C:/Users/Trilex/AppData/Local/Darkness2VR/packages/weapon-wheels-v2-20261031T231812860`; rollback `rollback/Restore.ps1`.
- Offline: dual_weapon_wheel MSVC /W4 /WX 121 PASS; protected build PASS (no warnings). Headset acceptance PENDING.

## Previous installed MAIN candidate: dual weapon wheels + crouch toggle (2026-10-31 23:05 Europe/Warsaw)

User ran preflight + install with the game closed; no agent launch.
- Source: `c80df956`, branch `main-weapon-wheels` (on `codex/hud-head-tentacles` e901b6ab).
- Installed DLL SHA256: `AA7E2EE863B19C52175425168C8964071CBAD6C8A16B2101DD07CB8106F4F510` (protected-native).
- Hold R3 -> dual hand wheels, stock DPAD equip with native verification; right stick down -> crouch ([Input] CrouchButton, default R3).
- Package: `C:/Users/Trilex/AppData/Local/Darkness2VR/packages/weapon-wheels-20261031T230525324`; full nine-target rollback `rollback/Restore.ps1`.
- Offline: dual_weapon_wheel MSVC /W4 /WX 139 PASS; protected build PASS. Headset acceptance PENDING.

## Previous installed lighting diagnostic completion (2026-10-31 21:43 Europe/Warsaw)

User confirmed game closed; authorized MAIN deployment completed without launch.
- Source: `e901b6ab`, branch `codex/hud-head-tentacles`.
- Installed DLL SHA256: `C306DA8AA8008514F1672BFC27628A799358A3C4AEE6B002241F2141A98A5C20` (protected-native).
- Adds effective lighting PS constants, reserves lighting shader slots, excludes post-source restoration from hand upload witnesses. No lighting fix claimed.
- Package: `C:/Users/Trilex/AppData/Local/Darkness2VR/packages/matched-header-20261031T214350385`; full nine-target rollback in `rollback/Restore.ps1`.
- Exact EXE/DLL verification PASS; four profiles and four caches preserved byte-for-byte. Gate39/validator32/build PASS offline. New capture acceptance pending.

## Previous diagnostic recorder correction (2026-10-31 21:30 Europe/Warsaw)

Authorized MAIN installation, game closed; no agent launch.
- Source: `2f027f05`, branch `codex/hud-head-tentacles`.
- Installed DLL SHA256: `469F7B92F316C34DBE05C4DDBE4707C4122C568D4EE64E99478494B2B90D03A7` (protected-native).
- Same-frame native camera header timing corrected; includes prior HUD/tentacle and AdaptiveFPS default changes.
- Package: `C:/Users/Trilex/AppData/Local/Darkness2VR/packages/matched-header-20261031T213047682`.
- Full nine-target rollback: package `rollback/Restore.ps1`.
- EXE and installed DLL identities verified; all four INIs/four caches preserved byte-for-byte. Existing AdaptiveFPS=OFF retained.
- Offline gate34/validator31/build PASS. Runtime capture acceptance and world-lighting fix OPEN; user launches manually.

## Previous profile correction (2026-10-31 21:16 Europe/Warsaw)

Both primary INIs now explicitly set Render/AdaptiveFPS=OFF. DLL unchanged:
155D6A427E4D53AD5EBEAE206E17C4A7F755ABBD4FEBC2C8D5629FAFF48D1AE1.
This removes the inherited AUTO half-rate cap on next launch; remaining stutters
and head-following world lighting are unresolved. No game launch/visual acceptance.
Full rollback/evidence: C:/Users/Trilex/AppData/Local/Darkness2VR/packages/hud-fps-off-20261031T211614129.
Previous INI hashes below are historical; current hashes:

- `C:\Program Files (x86)\Steam\steamapps\common\Darkness II\Darkness2VR.ini`: 047601C0B062449398E089E06275E71A87E534695EE9D6CBEEA2E4EDF9D4601F
- `C:\Users\Trilex\AppData\Local\Darkness2VR\Darkness2VR.ini`: 13274A3D5AA4549F03FD6B3D6E8D532B3720BF7D01831450B83ED68AC6475A07

## Previous installed MAIN candidate: native HUD / head-attached tentacles

Authorized MAIN deployment: 2026-10-31 21:03 Europe/Warsaw. User launches manually.
- **DLL SHA256**: 155D6A427E4D53AD5EBEAE206E17C4A7F755ABBD4FEBC2C8D5629FAFF48D1AE1 (protected-native)
- **Source**: codex/hud-head-tentacles, implementation 5e70b9db on main cb795d58.
- **Changes**: native owned HudStatus visibility override; existing mesh-scoped HeadAttachedTentacles enabled, preserving its acquisition/hold exclusion. Both primary INIs set HideNativeHUD=1 and HeadAttachedTentacles=1. All other settings retained.
- **Package/rollback**: C:/Users/Trilex/AppData/Local/Darkness2VR/packages/hud-head-tentacles-20261031T210354632; rollback/Restore.ps1 restores nine SHA-verified predeployment targets.
- **Validation**: standard/protected x86 builds, HUD467, actual tentacle26629, six production EXE pins, profile gate and installed hashes/INI read-back PASS. No game launch or HMD acceptance; this is a candidate, not an accepted baseline.
- **Handoff**: docs/agent-workflow/handoffs/HANDOFF-hud-head-tentacles.md.

- **Current INI SHA256** (`C:\Program Files (x86)\Steam\steamapps\common\Darkness II\Darkness2VR.ini`): 12E6E973384074DADF6C5D31A7A4EFB19541643471A0DEEFCA32C5F0493ED635
- **Current INI SHA256** (`C:\Users\Trilex\AppData\Local\Darkness2VR\Darkness2VR.ini`): 9501909EE3820B181D3C4184FEC388C19B27E9A2190C082860C5CB64F30B6C01

## Previous MAIN candidate: X double tap toggle + left trigger DOWN/UP

Authorized MAIN deployment completed 2026-10-31 20:41 Europe/Warsaw (DLL only).
- **Current DLL SHA256**: 6CA7A1F5A40E29FBD7451CD7051D72C170D9FF33C16CE589ED968A882541AF80 (protected-native)
- **Source**: side/unarmed-trigger-dpad-on-installed ee40c864
- **Behaviour**: X double tap alternates DPAD_RIGHT/DPAD_LEFT (headset 20:39: switching confirmed). Left trigger with an empty left hand alternates DPAD_DOWN/DPAD_UP; right trigger always fires. UnarmedTriggerDpad defaults to 1 (Steam INI has no key).
- **Package**: C:/Users/Trilex/AppData/Local/Darkness2VR/packages/unarmed-lefttrigger-20261031T204143841
- **Validation**: MSVC x86 build PASS, host tests PASS, profile gate PASS; headset NOT RUN for the trigger change.

## Previous MAIN candidate: X double tap weapon toggle (pure alternate) (superseded)

Authorized MAIN deployment completed 2026-10-31 20:36 Europe/Warsaw (DLL only).
- **Current DLL SHA256**: 15D89E52DFB304F2FE3ABD4C6CF980718E13872927770AB7560674B8A9980994 (protected-native)
- **Source**: side/unarmed-trigger-dpad-on-installed bb716c2e
- **Change**: TOGGLE alternates DPAD_RIGHT/DPAD_LEFT; native firearm kind ignored (log 20:35: right=5 for every held weapon).
- **Package**: C:/Users/Trilex/AppData/Local/Darkness2VR/packages/unarmed-toggle3-20261031T203623295
- **Validation**: MSVC x86 build PASS, host tests PASS, profile gate PASS; headset NOT RUN.

## Previous MAIN candidate: X double tap weapon toggle (automatic = alternate) (superseded)

Authorized MAIN deployment completed 2026-10-31 20:32 Europe/Warsaw (DLL only; INI already DoubleTapX=TOGGLE).
- **Current DLL SHA256**: 98827196A74243B28CA06EA39C942322977A373AFD3FDAB927BBD779F8C26342 (protected-native)
- **Source**: side/unarmed-trigger-dpad-on-installed 0b91c281
- **Change vs F5046F99**: raw Automatic firearm (uzi and automatic rifles) is Unknown -> alternate RIGHT/LEFT; log shows raw firearm codes.
- **Package**: C:/Users/Trilex/AppData/Local/Darkness2VR/packages/unarmed-toggle2-20261031T203205049
- **Validation**: MSVC x86 build PASS, host tests PASS, profile gate PASS; headset NOT RUN.

## Previous MAIN candidate: X double tap weapon toggle (superseded)

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
