# Native HUD and head-attached tentacles candidate

Base: cb795d58, user-selected current main. Role: MAIN for deployment only;
user will launch the game. Both requested changes remain HMD-unverified.

## Hypothesis and plan

The existing mesh-scoped HeadAttachedTentacles path is present but both installed
primary INIs disable it. Enable that existing correction without broadening its
mesh/shader/hold gates. Hide gameplay HUD through its native HudStatus visibility
consumer, never through the shared 7070C5BC UI shader or a gameplay-mode heuristic.
Build and validate offline, then deploy only after the user's explicit approval.

## Evidence (stock EXE only)

EXE SHA256: D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1.
Private extracted H_Script/D2/Menus/HUD.lua: line247 gets the first local
player's GetHudStatus; lines3184-3204 consume IsVisible with the profile's HUD
setting and change movie alpha. Source assets/decompilation are not committed.
SWIG Player GetHudStatus registration: 010BA224 -> DD0FA0 -> 7928C0.
Player GetAvatar registration: 010BA21C -> DE90C0 -> 43BCB0.
Avatar GetPlayer registration: 010B9898 -> DE7A00 -> 6B0CD0.
HudStatus IsVisible/SetVisible wrappers DCF600/DCF6C0 call 819F60/C8B380.

- 6B0CD0: `8b 89 e4 04 00 00`: Avatar player reference at +4E4.
- 43BCB0: `8b 49 74`: Player avatar reference at +74; require the backlink.
- 7928D2: `8b 47 60 83 38 00 8d 5f 60`: existing HUD reference at player+60.
  Do not invoke this getter's lazy allocation branch.
- BFA14C: `c7 06 88 3b fe 00`: HudStatus constructor table FE3B88.
- C8B380: `8a442404 88813c010000 c20400`: visibility setter writes one byte +13C.
- 819F60: native visibility predicate combines this +13C byte with native
  HudVisible global 010FDC54, updates +13D and returns the result.
  The global and the predicate's side-effect byte are never written by the mod.

Production checks these six ranges once after existing native owner/EXE admission.
The callback's selected-avatar strong reference owns the borrowed player/HUD
chain on the native render thread; no object pointer is dereferenced after the
callback. Require the player/avatar backlink, exact HUD type and image write guard.
Only the owned heap visibility byte is overridden. No EXE/IAT/vtable changes.

HideNativeHUD missing-key default is OFF. Candidate profile explicitly enables it.
During native view callbacks outside active SubmitMode4/Stereo1 VR, the saved
visibility byte is restored only for the same current owner. Unknown or replaced
objects are untouched; no restoration dereferences an old object. Exit/restart
naturally discards process-local state. Native scripts may update the flag; the
next admitted view enforces hiding. No global shader suppression is added.
Actual menu/cutscene/subtitle behavior, callback-to-Lua ordering and complete HUD
coverage must be verified in game. A read-back of zero alone is not acceptance.

## Tentacles

Enable existing HeadAttachedTentacles=1 in the candidate. Keep fresh Empty native
grab feedback and the seven captured VB offsets/302920-byte VB/213588-byte IB,
stride40 and three shader gates. Acquisition/holds stay in native world space:
previous headset evidence showed a displaced held target otherwise. Do not
head-attach NPCs or move held-target physics. Existing hand/weapon aim is retained.

## Offline validation

- Both standard and protected-native MSVC x86 DLL builds PASS.
- Native HUD owner/override host fixture: 467 checks PASS (/W4 /WX).
- Production HUD pin verifier: exact EXE SHA256 + six pin ranges PASS.
- Actual-source tentacle draw scope: 26,629 checks PASS.
- Profile gate requires protected-native flavor for HideNativeHUD.

## Manual acceptance (user launches)

1. Load gameplay: crosshair, health/ammo HUD should disappear; check both eyes.
2. Open pause/options, then close: menus remain usable, HUD stays hidden.
3. Turn/lean head: idle tentacles follow; NPCs remain world-relative.
4. Grab a prop/enemy and move head: native hold stays connected; release restores
   idle head attachment. Test equipped weapons and existing trigger alternation.
5. Report subtitle/objective/cutscene behavior and any discomfort/regression.

Rollback: complete verified predeployment package, recorded in the task handoff.
No game launch, Steam test or physical-headset acceptance by the agent.

## User follow-up: world lighting follows head / FPS drops (21:10 session)

User reports the light/shadow patches on walls and floor move with head motion
(not merely tentacle shadows). Screenshot is a stationary view of the station
corridor; it cannot identify the producing shader or prove temporal motion.
Treat world-lighting bug as OPEN. Do not weaken head tracking or globally suppress
shadows. Existing C4D1FB10 screen-depth experiment is unproven and stays OFF.
No speculative shader/atlas patch was deployed.

Retained evidence: private before-d3d9.log in the correction package below,
original session PID36340. Startup line10 explicitly requested AdaptiveFPS=AUTO;
both installed primary INIs omitted AdaptiveFPS, inheriting source fallback AUTO.
HALF applied at frames1431/1500 and20795/21000. At frame21000 policy measured
43 app FPS; nearby XRCadence window reported42.1 layers/s and40.1 world-bearing
boundaries/s,32ms boundary p95,22.222ms runtimePeriod,90Hz physical refresh.
This demonstrates an unwanted application cap, not the cause of every hitch.
The inherited main default and previous candidate both retained this behavior.

Other stalls remain: CPUSync window ending around frame17072 contains a31.629ms
producer fence,35.053ms Present-boundary maximum; earlier producer maximum was
59.845ms. These waits do not establish GPU Busy or prove a compiler/texture cause.
The2651.668ms maximum at startup is not steady-gameplay frame-time evidence.
No broad FPS claim or unsupported CPU micro-optimization is made.

Correction: installed AdaptiveFPS=OFF explicitly in both primary INIs, with full
nine-target DLL/INI/cache backup and unchanged DLL155D6A42. Source missing-key
fallback and repo candidate default now OFF, and future startup logs include OFF.
No game launched. Next process launch is required for the installed INI change.
Package: C:/Users/Trilex/AppData/Local/Darkness2VR/packages/hud-fps-off-20261031T211614129
Rollback: package/rollback/Restore.ps1. It restores the preceding candidate, which
had inherited AUTO; this rollback is for recovery, not the desired FPS setting.

Validation: adaptive policy3253 checks PASS; protected head inverse/reconstruction
117021 checks PASS. Adaptive integration executable compiled but execution was
blocked by a sharing lock after the existing bounded retry; not a PASS. Initial
DLL link failed with LNK1101/MSPDB140 mismatch in the restricted environment;
same build command outside that restriction succeeded. New source DLL built
but NOT installed: the old installed binary honors explicit OFF without rebuilding.

Next evidence: user loads this same corridor, stationary body/stick/mouse, and
provides two distinct physical head orientations. Use the existing one-frame
matched capture only while loaded, one explicit serial per pose, then restore
capture settings. Require same-frame effective head transforms, source/final eye
images, shaders, draw banks and light/depth inputs, plus completion receipts.
Do not activate legacy EnginePresentCallsite from outdated capture instructions:
current protected native Present ownership is already installed. Capture readbacks
are diagnostic and must not be used as normal performance measurements.

## Recorder follow-up: same-frame native camera

After user scene readiness, one serial101 capture of PID35892/XRframe6108
was requested. It was rejected: `selected native camera changed after capture
header; request a stable frame`, followed by missing same-frame projection
witnesses. Only three files exist; no COMPLETE receipt or images. The validator
correctly rejects it. Output remains private under the process temp directory.
Diagnostic INI was restored to its original bytes after the one request.

The recorder armed at the preceding Present and wrote eye parameters before
the selected native camera callback refreshed this frame's matrices. This
legitimate refresh was then rejected as a post-header camera change. Defer
header/config serialization until the first capture consumption, requiring the
current native camera frame. Existing device/Present/reset/XR/config guards run
first; any later camera change still rejects. A failed request cannot recover.
This changes opt-in diagnostic recording only, without a lighting workaround.

Validation: 34 actual-source capture gate checks (x86), 31 existing validator
tests and protected diagnostic build PASS. Restricted-temp Python execution
failed on ACL access; identical tests outside that restriction passed. Candidate
DLL SHA256 469F7B92F316C34DBE05C4DDBE4707C4122C568D4EE64E99478494B2B90D03A7.
World-lighting and actual runtime capture acceptance are still unverified.
