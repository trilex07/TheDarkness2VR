# Baseline and current MAIN candidate

## Accepted Steam baseline: outline coverage and scoped shadows (2026-11-04)

User tested the installed EdgeShadowExperiment and reported "tak jest dobrze",
then requested main integration. Functional headset acceptance recorded; this
does not establish exhaustive coverage of every light/cutscene or an FPS gain.
Source3a6113ba6ab80cbaa06ec78e28100bd48c5fec12, integrated into main from
codex/edge-shadow-stability-20261104 in MAIN worktree .main-camera-shake.
Installed DLL51D08FD328EB11CDD391CD4E457C37587B97983A3F3611FDC0F8644883A6230A;
diagnostic INI117230AC717C2A5FE7C1CA82BF6A693B2A056855D4B47F18CCA941325462C076.
ShadowClipCoordinates=1 and OutlineCoverageExperiment=1; simulator/collectors OFF.
Native outline coverage integrates four subpixel samples without full-colour
blur. Exact shadow consumer uses corrected NDC only with the geometric-head
inverse, preserving native input in other camera contexts. Prior VR settings,
Polish font fixes and accepted render/input features retained.
Main INI22862511B4E5697F37413802D3B562DD388ED510C68D0E1CD745E629A5A79B08
is the user's current SS125 configuration; resolution/NIS unchanged by this task.
EXE/loader unchanged. Install/VerifyOnly/Restore/VerifyOnly passed before trial;
deployed hashes independently rechecked at acceptance. No deployment on merge.
Frozen package build/edge-shadow-stability-20261104/Install.ps1:
-Mode EdgeShadowExperiment reinstalls this accepted profile; -Restore rolls back
to DLL95BB1C37/diagnostic3A3FD698 with the game closed. Default Combined mode
keeps experiments OFF; choose the accepted profile explicitly when reinstalling.
Evidence handoffs/HANDOFF-edge-shadow-stability-20261104.md. Older baseline
sections below are historical. No claim that every shimmer/shadow issue is solved.

## Accepted Steam baseline: VR settings and Polish font overlay (2026-11-04)

Source324a227841156ed9c9d84718460d0f7120f06d68 on
codex/vr-settings-polish-fonts-20261104, MAIN worktree .main-camera-shake.
DLL95BB1C3706D95CA0AE4A659BC497216E8C1EC757AED7CE692FA61B0BA122F286;
main INI C0AF553CA14EF17C5ED22F89BA40377094F3710BBA07D07B005C72636629F72A;
diagnostic INI3A3FD69804D0E62A66E699652AB89C6BCD086CCB271CBE31A65B97F8071E6A6A.
Main/pause VR settings: independent PL text/voice, live NIS, restart SS, controls.
SS100, PL/PL, NIS100 defaults; simulator/collectors OFF. DLC H/B.Font tables
synchronized with current base font atlases; base fonts/textures/scripts and
EXE/loader unchanged. Emulator menu/language/restart/SS/font A/B PASS; headset
acceptance was initially NOT RUN. User subsequently accepted the working set
and requested integration into main on 2026-11-04. This is functional acceptance;
exhaustive dialogue inspection and FPS/shimmer/shadow fixes remain unverified.
Prior baseline below remains rollback. Full33-target Restore and reinstall/VerifyOnly PASS.
Package build/vr-settings-fonts-candidate-20261104/Install.ps1 (-Restore rollback).
Handoff handoffs/HANDOFF-vr-settings-polish-fonts-20261104.md. User4 save/config
hashes restored; production candidate is installed with game closed.

## Current Steam: Combined with fixed-center NIS fallback (2026-11-04)

User accepts the scene/input candidate but rejects SMAA effectiveness; continue
shimmer/FPS investigation. Closed-game deployment of source8ac9f861abecc8d01e35e6529c5c1fbbc63ec62d
uses build/combined-gaze-stability-20261104, ModeCombined. SMAA/FXAA OFF;
fixed-center missing-gaze fallback defaults ON. NIS strengths, render resolution,
main INI and protected EXE/loader preserved. Shadow/reflection experiments OFF.
DLL4A0320B2C9369EA8696C65786B4A871434D855CB62540859209DC95A4F559EC7;
diagnostic INI3A3FD69804D0E62A66E699652AB89C6BCD086CCB271CBE31A65B97F8071E6A6A.
Frozen install/VerifyOnly PASS. Exact rollback in that package. No game launched.
Actual NIS host blink regression FAIL before/PASS after on WARP and RTX5070;
both x86 builds PASS. New fallback HMD acceptance NOT RUN. User40FPS/25ms case
and complete geometric/specular shimmer remain unresolved; no new FPS gain claim.
The previous installed SMAA package below is historical, not current Steam.

## Consolidated SMAA candidate installed for user test (2026-11-04)

User authorized installing the consolidated candidate. Game was closed.
Package: build/combined-scenes-smaa-20261104, source33fe966d530804b497aadc64d9404aaf6796b0a1.
ModeSmaaExperiment: Combined scene/input fixes plus SMAA after NIS; FXAA OFF,
shadow/reflection experiments OFF, simulator/collectors OFF. Main INI and
protected EXE/loader match original hashes. Installed DLL
2119EA3CF014FE5C1823452C8E71D298FEF3ECFD8290BF0527A4508A49B92576;
diagnostic INI ECD05B9997B38A515FF729114314928741904E4F9EDB01DC416880B04B716000.
Install and VerifyOnly PASS; no game launched. HMD acceptance PENDING,
FPS recovery and shadow/reflection visual fixes remain unresolved.
Private exact receipt: package installed.json. Rollback with that package's
Install.ps1 -Restore while game closed restores E56BF027/AC8290E4 below.
This is a deployed candidate, not a newly accepted baseline or main merge.

## Isolated eye output accepted for continued play (2026-11-03)

User reports "jest dobrze" after the isolated-eye-output Steam trial. Current
functional reference: source81bddf2b4ec734a8fab5d885e01099b17752606e,
DLL E56BF027875A6FF3997E0B6A51A0DC43F52DD86C2EB39AE79D4C5EAB073FA5CA,
INI68EE5685C7AE9FEC8F1AE2677D551DD90C7BCC21DD66737560815EEF4AB26EA9.
Receipt: .main-camera-shake/build/isolated-eye-output-20261103/installed.json.
This supersedes the Combined-only reference below for subsequent work; no
quantified FPS improvement, exhaustive scene acceptance or main merge claimed.
New gaze-FXAA/NIS work remains separate and undeployed.

## Combined accepted for continued optimization (2026-11-03)

User says "okej jest kontynuuj z kolejnymi duzymi optymalizacjami" after the
Combined Steam trial. Retain it as the current functional reference, without
claiming a quantified FPS gain or exhaustive scene acceptance. No main merge
was requested. Source d58156b45d89673534db135ee09b0cadc72b38d6; deployed DLL
FE216AF4E8B310C9D7E68EB059428194BA49B1FDE1C448CD1547F7E0419FF219;
INI 2537EC1EFE21509EC86AD18B1A878EBE607DB8B051A022C1D44C8DD3479CA14D.
Receipt and exact prior B97 rollback:
.main-camera-shake/build/render-pipeline-candidate-20261103/installed.json.
New isolated-eye-output work is separate, OFF by default and not deployed.
See HANDOFF-render-pipeline-candidate-20261103.md and
HANDOFF-isolated-eye-output-20261103.md. Historical entries below remain intact.

## Latest installed runtime accepted for continued play (2026-11-02)

User reports 'ta okej' on source f5bdb1e8, installed DLL
99DAF70040D3EC328D31FA3483B0C43B57DBBE7DC035EA4AAE1E50F209B295F7;
receipt cinematic-theater-20261102T182344252. This supersedes the rejected
FD6470F6 candidate below. Includes 47-bone peaceful hands and long L3+R3
minute-only VR/cinema override. Automatic Bink fullscreen classification remains
a known unresolved issue with the requested manual workaround. No new main
merge requested. Separate new issue: objective destination markers follow head.
See HANDOFF-world-objective-marker-20261102.md for the new task and button
candidate. Historical installed candidates below are retained for rollback.

## Installed villa cinema/hands and six natural dialogue lines (2026-11-02)

User follow-up FAIL: false cinema and rigid hands persist. Candidate is NOT
accepted for main. Live read-only diagnosis identifies Bink mode2/native0 as
the false-cinema route; hands publication empty, controller-motion capture
pending. See follow-up section in the per-task handoff. DLL/assets unchanged.

MAIN source44d9e7ba on codex/villa-cinema-hands-audio-20261102; DLL SHA256
FD6470F6B135A49DD77E30A023A5A931683025C719455EC3EAAA7220A01443AF.
Retains CinematicTheater=1 and DialogueCameraComfort=1. Theater now requires
running native cinematic without FreeCam; protected human hands tolerate absent
demon grab equipment. Six S03MansionI boss-crazy lines shortened/regenerated at
natural speed with corresponding subtitles. Elevator voice not identified.
Receipts under C:/Users/Trilex/AppData/Local/Darkness2VR/packages/:
cinematic-theater-20261102T170623530 and villa-natural-dialogue-20261102T1707.
Independent audit PASS: DLL2 backups/2 installed/4 protected; audio8 backups/
8 installed/19 protected (includes current fonts/textures). Closed-game install,
no launch. Offline build/cinema/hands/head/codec PASS; existing dialogue fixture
fixed-address mapping BLOCKED. Headset and listening acceptance NOT RUN.
Accepted main unchanged. Exact rollback/evidence in
handoffs/HANDOFF-villa-cinema-hands-audio-20261102.md.

## Installed NPC dialogue camera candidate (2026-11-02)

- Accepted cinema/controller retirement integrated and pushed to main/origin/main `897c5c44`. New dialogue candidate remains separate on `codex/dialogue-camera-comfort-20261102`, source `755270d0`, DLL SHA256 `E03CB97E0E99D753E52DF5440465A66949D2AB9D0DE4700C4F2FCB277C3E8DBE`.
- `Render/DialogueCameraComfort=1` blocks only the verified ConversationViewControlNode rotation setter chain on the selected local camera, before native composition. Retains cinema, native body/position, no-shake model snapshot, haptics and other setter callers. Defaults OFF; does not zero a pre-existing native angle or change recenter.
- Build and actual x86 caller-chain/regression tests PASS; physical dialogue/exit/stick-turn/cinema NOT RUN. Independent installation audit PASS:2 backups,2 target hashes,4 protected hashes; only INI addition `DialogueCameraComfort=1`; game closed and not launched.
- Receipt/rollback: `C:/Users/Trilex/AppData/Local/Darkness2VR/packages/cinematic-theater-20261102T094859859`. `tools/deploy-cinematic-theater.ps1 -Restore <package>` restores accepted DLL3F575CB4 and exact prior INI with game closed. Evidence: `handoffs/HANDOFF-dialogue-camera-comfort-20261102.md`.

## Accepted cinematic theater and controller retirement (2026-11-02)

- User physically accepted the installed cinema/controller-retirement candidate and requested main integration: "jest okej wrzuc na main". Accepted DLL `3F575CB4BD277064510CA6AD9A528FA70FFACE91387055351D340EBB0EDA75B1`, source `c1ac5b8d`; Render/CinematicTheater=1. This supersedes pending acceptance below. Repo profile now enables the accepted feature.
- Separate new issue: NPC conversation forces downward/tilted camera and leaves that orientation after dialogue; L3+R3 does not straighten it. User screenshot and report open a separate dialogue-camera task. Do not treat theater acceptance as resolution of dialogue control.

## Installed cinematic controller retirement follow-up (2026-11-02)

- Source `c1ac5b8d`, DLL `3F575CB4BD277064510CA6AD9A528FA70FFACE91387055351D340EBB0EDA75B1`. During native cinema, clear/reject controller arm poses and restore only our arm-palette edits independently of tracking/pause. Native full-body animation/weapon transforms retained. Actual production x86 pack/restore regression PASS; physical appearance NOT RUN.
- Receipt/rollback: `C:/Users/Trilex/AppData/Local/Darkness2VR/packages/cinematic-theater-20261102T085451951`. Independent audit PASS:2 backups,2 installed hashes,4 protected hashes; INI byte-identical to previous cinema candidate. Game closed during install; not launched. Restore this package returns preceding cinema candidate; restoring preceding package returns accepted DLL9AF9A51E.

## Installed cinematic theater candidate (2026-11-02)

- MAIN installed source `f247d108` on `codex/cinematic-theater-20261102`: DLL SHA256 `3126053204B354D99B0CB0D8BEBC8A0178C2031E5D5C34CA7CC3F17BA46F4374`, game INI `Render/CinematicTheater=1`. Native cinematic signal selects room-anchored stereo screens; codec Wait/Close observer selects mono Bink screen. Default in source remains OFF.
- Offline build/tests PASS; independent installation audit PASS:2 preimage backups,2 installed hashes,4 protected runtime hashes, exactly one INI key changed. Game closed throughout; no Steam launch/input. Accepted camera/haptics/model code and font/audio/texture assets retained. Physical cinematic/intro detection, framing and transitions NOT RUN; this is a candidate, not accepted main.
- Receipt: `C:/Users/Trilex/AppData/Local/Darkness2VR/packages/cinematic-theater-20261102T084535813/deployment.json`. Closed-game rollback: `tools/deploy-cinematic-theater.ps1 -Restore <that-package-directory>` restores accepted DLL9AF9A51E and exact prior game INI. See `handoffs/HANDOFF-cinematic-theater-20261102.md` for evidence and bounded coverage.

## Accepted texture and Polish font repair (2026-11-01)

- User accepted the final installed font geometry correction: "dobra moze byc daj na main". This supersedes the pending font acceptance entries below. Corrected64 comic model textures were accepted separately during the same test sequence; final font source is `44cb00b9`, on top of native-cell source `fcee7c8f`.
- MAIN integrates the complete repair branch into Git main by fast-forward. Accepted camera/haptics/model DLL `9AF9A51E7E517621C1C667E192D17B158C2F4DBCE1256F98AA9DEFD79C441A5D`, updated Polish audio/text and current configuration remain unchanged. This is acceptance of the tested result, not an exhaustive linguistic/audio audit.
- Deployment provenance and verified rollback paths remain in `handoffs/HANDOFF-texture-font-repair-20261101.md`. Integration is source/documentation only; no additional game installation or launch is needed.

## Latest font geometry candidate (2026-11-01 23:18 Warsaw)

- User grades native-cell-v3 "prawie": text readable, accented geometry still uneven. Found pre-existing six font families also had oversized PL UV spans and advance+1. Installed source `44cb00b9` normalizes those108 earlier glyphs to donor cells/metrics; all non-PL UVs and decoded glyph pixels are verified unchanged.
- Four-file font-only install/audit PASS:4 backups,4 installed hashes,23 protected runtime/audio/text/model/font-header files unchanged. Accepted64 model textures remain unchanged. Physical final font geometry acceptance PENDING.
- Receipt `C:/Users/Trilex/AppData/Local/Darkness2VR/backups/font-metrics-six-20261101-v1/deployment.json`; manifest SHA256 `20e0e54d9aae8330a624436160040ea4bfda6c6f9c630ebb5204b9bf8beb4510`. Hash-gated closed-game rollback uses `tools/deploy-asset-repair.py rollback` with that backup and explicit game directory. Steam launch requested23:18:40; no input injected.

## Current: textures accepted, revised font candidate (2026-11-01 23:14 Warsaw)

- User accepts corrected new comic model textures. Six H/B/F.Texture files remain byte-identical through subsequent font-only updates. The enlarged-atlas font attempt failed physical acceptance; in-place-v2 improved the title but its screenshot still showed uneven new glyphs.
- Installed source `fcee7c8f`, private `build/font-native-cells-v3`: original atlas dimensions, existing UV/pixels and metrics retained; newly drawn Polish glyphs also preserve their native donor's full cell, UV span and half-texel phase. Font appearance remains PENDING user test; do not claim resolved from offline proof.
- Independent install audit PASS:14 backups,14 installed archive hashes,13 protected runtime/audio/text files; six accepted model texture archives unchanged. Game closed during installation. DLL9AF9A51E remains accepted; audio/text and INIs unchanged.
- Receipt/rollback backup: `C:/Users/Trilex/AppData/Local/Darkness2VR/backups/font-native-cells-20261101-v3/deployment.json`. Repair manifest SHA256 `4c625aee5f915b78d8a4386caeff92935a1f63729701fa0880ea7d5ae0e3a7b0`. Closed-game rollback uses `tools/deploy-asset-repair.py rollback` with that backup and the explicit game directory; it restores the preceding font-only candidate and retains model textures. Steam launch requested23:14:51; no input injected.
- Current source/deployment evidence is on `codex/texture-font-repair-20261101`; main remains `564f8342`. No unaccepted font candidate has been merged into main.

## Installed texture/font repair candidate (2026-11-01 22:57 Warsaw)

- Previous asset update failed physical acceptance: rainbow artifacts on hands/tentacles and absent Polish gameplay-caption glyphs. Camera/haptics/model DLL9AF9A51E remains accepted and unchanged.
- Installed data-only source `ef56b259`: correct standard block storage for64 uncompressed comic textures and add342 Polish glyphs across19 omitted Latin fonts (including Corbel). Original six already-patched fonts retained. Independent Pillow decode comparison64/64 PASS (maximum mean RGB error7.837/255), original font atlas region/metrics/other languages preserved. Physical repair acceptance pending.
- Closed-game install and independent audit PASS:14/14 preimage backups,14/14 installed archive hashes,13/13 protected runtime/audio/text files unchanged. Repair manifest SHA256 `560a7052651824c844ad06a9d1b6ec9e7f919f44ac859c4ddc9810c882dc23be`.
- Backup/receipt `C:/Users/Trilex/AppData/Local/Darkness2VR/backups/texture-font-repair-20261101-v3/deployment.json`. With game closed: `python -B tools/deploy-asset-repair.py rollback --backup <that-directory> --game "C:/Program Files (x86)/Steam/steamapps/common/Darkness II"` restores the preceding rejected asset version. The earlier22-file backup can instead restore pre-update assets.
- Details: `handoffs/HANDOFF-texture-font-repair-20261101.md`. Steam startup requested after audit; no input injected.

## Installed Polish update and comic textures (2026-11-01 22:40 Warsaw)

- Following user acceptance and integration of the camera/model fix into `main` (`370897d3`), MAIN installed the prepared private Polish complete release (16 archive files) and 64 comic weapon/tentacle textures. Runtime DLL remains accepted `9AF9A51E7E517621C1C667E192D17B158C2F4DBCE1256F98AA9DEFD79C441A5D`; EXE and all three INIs unchanged.
- Independent audit PASS: 22/22 preimage backup hashes, 22/22 installed archive hashes, 5/5 protected runtime files. Texture readback verifies 64 H + 64 B + 62 F payloads and preserves every original cache byte and unrelated TOC field. This is installation/integrity evidence; the new audio and texture appearance still require in-game user acceptance.
- Evidence/rollback: `C:/Users/Trilex/AppData/Local/Darkness2VR/backups/polish-comic-main-20261101-accepted-camera/deployment.json`. Closed-game rollback: `python -B tools/deploy-polish-comic.py --rollback --backup <that-directory>`. Restore returns all 22 archives to the exact pre-update state and leaves the accepted camera DLL/configuration intact.
- Package provenance and startup results: `handoffs/HANDOFF-polish-comic-main-20261101.md`.

## Accepted camera and first-person models (2026-11-01)

- User physically tested DLL `9AF9A51E7E517621C1C667E192D17B158C2F4DBCE1256F98AA9DEFD79C441A5D` (source `eda9420e`) and confirmed "jest okej", requesting integration into main. This accepts the model snapshot correction together with the previously confirmed camera stabilization and event vibration.
- MAIN verified the installed DLL still matches that hash. Preserve both enabled features and the existing shotgun profile. User next requests installation of the prepared Polish localization/dubbing update and 64 comic weapon/tentacle textures; their runtime acceptance is separate.
- Git `main` is the requested integration target. The former `codex/gaze-nis-main-integration` branch remains historical; this task branch contains its source history.

## Installed model snapshot correction (2026-11-01 22:26 Warsaw)

- Source `eda9420e`, DLL `9AF9A51E7E517621C1C667E192D17B158C2F4DBCE1256F98AA9DEFD79C441A5D`. Native stabilized view now refreshes the first-person model snapshot on every dispatch, including the ordinary path. Camera stabilization and event rumble retained; physical model result pending.
- Game closed as explicitly confirmed by user. Independent audit:9/9 backups verified,8/8 other protected targets unchanged (including diagnostic INI), exact DLL and stock EXE. Diagnostic hash remains`AC8290E48D6CD8CA34ED935734E7293572E2F9227B0AA27D4CA03507FA619299`.
- Rollback with closed game: `C:/Users/Trilex/AppData/Local/Darkness2VR/packages/camera-model-snapshot-20261101T222624275/rollback/Restore.ps1`, restoring the preceding61C4270D candidate. MAIN requests Steam startup smoke after verification.

## Installed native source camera candidate: partial acceptance, model regression (2026-11-01)

- Installed source `79c3e7ee52a01d33ca871db1d5b7bb87e30d3065`, DLL `61C4270D5C9DF9475A8A53C55AA4FD5C122408F51626CDDDC7AB3F305F74CB37`, diagnostic INI `AC8290E48D6CD8CA34ED935734E7293572E2F9227B0AA27D4CA03507FA619299`. Both `CameraNativeNoShake=1` and `CameraEventRumble=1`. Shotgun retained, rejected stabilizers OFF, EXE unchanged.
- Installed at22:17 Warsaw with game closed; independent9/9 backups and7/7 other protected targets unchanged. Rollback: `C:/Users/Trilex/AppData/Local/Darkness2VR/packages/camera-native-no-shake-20261101T221730972/rollback/Restore.ps1` (restores haptics-only8DB11F12...). MAIN launched via Steam; PID38032. Runtime selected owner334ABCA0/avatar333D8F10; private dispatch published and count10800+, `animationBypass=0` (ordinary direct path; don't claim channel adapter activation).
- **User physical feedback**: no visible camera shake; vibration present. **Regression**: tentacles stay in world, weapon appears/disappears. Two attached screenshots show displaced tentacles and floating weapon. This is NOT a fully accepted baseline. User asks to preserve stabilization and fix models next.

## Installed selected-owner event rumble candidate (2026-11-01 22:03 Warsaw)

- **Installed DLL SHA256**: `8DB11F12A5032BCAC7FA4806C22E1439DDE63BE5035B244159FDD043EF1A8966`, source `63adb43b5d6130dc83757c9a328aed3e675a3176` on `codex/camera-shake-source-main-20261101`. Diagnostic INI SHA256 `D218DB73AF6452218E9AE2829767869304F05EE294A3A88354EC00385FE963F3` enables `CameraEventRumble=1`; shotgun and native image guard retained, rejected shake paths OFF.
- **Scope**: read-only selected-owner impact/environment feedback through existing OpenXR haptics. Camera shake is still unresolved. Offline fixture/replay PASS 50,624 checks; physical controller acceptance NOT RUN.
- **Deployment**: user confirmed game closed and authorized installation. Process checks passed; independent audit verified 9/9 rollback backups, 7/7 other protected targets unchanged, exact DLL/INI hashes and stock EXE `D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1`. Game not launched by agent.
- **Rollback**: `C:/Users/Trilex/AppData/Local/Darkness2VR/packages/camera-event-rumble-20261101T220349965/rollback/Restore.ps1`, with game closed; restores prior shotgun/trace DLL `7414984F...` and diagnostic INI.

## Rejected animation stabilizer restored by MAIN (2026-11-01)

- **Current installed DLL SHA256**: `7414984FF998FABDF154AEB5EE443D75805378DDB2359F7EC6F20E83E8A6FA55`, owner-trace/shotgun baseline described below. MAIN ran `camera-anim-stabilize-20261101T212812295/rollback/Restore.ps1` with the game closed. Independent post-restore verification matched all nine protected targets against that rollback manifest; stock EXE remains `D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1`.
- **Rejected DLL**: `A6BDC3043B495D73FDDE0EA86D3BB2910AF19A7A7DC102FC2D482F42DA17C683`. Latest user feedback: walking vibrates, camera continues shaking including train, damage produces no vibration. Do not redeploy the stabilizer/source-suppression package as a working fix.
- **Current work**: `codex/camera-shake-source-main-20261101`, based on `3c19e744`, in `.main-camera-shake/`. No new runtime change is installed. A separate read-only event sampler observes all candidate noise owners and impact output fields on the restored DLL. See `handoffs/HANDOFF-camera-shake-source-main-20261101.md`.

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
