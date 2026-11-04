# Handoff: tentacle targeting, eye attachments and camera leveling

Date: 2026-11-04. Role: MAIN.
Task: tentacle-aim-light-recenter-20261104.
Branch: codex/tentacle-aim-light-recenter-20261104.
Worktree: .main-camera-shake. Source baseline: main 8c9ed94f.
Accepted baseline: DLL 51D08FD328EB11CDD391CD4E457C37587B97983A3F3611FDC0F8644883A6230A;
diagnostic INI 117230AC717C2A5FE7C1CA82BF6A693B2A056855D4B47F18CCA941325462C076.
User's prior edge/shadow acceptance was already integrated and pushed to main.
These new changes require headset acceptance before main integration.

## Changes and evidence
- protected_left_aim.inl publishes a fresh acquisition ray before LB/grip, since
  the native target query may run before that pulse. Valid gaze has priority,
  independent of left-hand tracking. Fallback requires the left controller AIM
  pose, not merely grip validity. Cinema, special avatars, QTE/action movies,
  pause, alternate held target and stale frame/ownership guards remain active.
- Native caller C63011 consumed the controller acquisition ray in emulator tests.
  Real PSVR2 bridge remained running and reported invalid eye samples (flags 38).
  It was not overwritten by a synthetic producer; physical valid gaze NOT RUN.
- Eye glow pixel provenance: VS 7070C5BC, complete SHA256
  60aa9a279fb5b7f34a688838bd9bbda1d5a52ce1409856fef76c5a5412e8743b;
  PS 594C16EE; FP viewport depth 0..0.05; perspective c0..c3;
  256x256 DXT1 one-level texture, native payload FNV1a 2ECBF95D.
  World particles share this shader pair: shader identity alone is insufficient.
  Cancel head clip only for the exact texture and permitted native tentacle state,
  restore actual constants after each draw. Cover all four primitive/indexed/UP
  paths: native left uses DrawPrimitive, right replay uses CPU/UP data.
  Cache texture identity on the COM resource; transfer from CPU upload source,
  avoiding recurring GPU readbacks. Successful unrelated uploads revoke identity.
- Recenter now queues a one-shot, owner-bound request (500 ms expiry), consumed
  before admitted FP composition in the native camera-source dispatch. Preserve
  native yaw, zero pitch and roll through verified native thiscall setter 9D9120,
  length 0x9a, SHA256
  9fc2b97094c608861c2672f4c73aa517fa70438f4e353b8d73ff98f8d3945a58.
  camera+110 getter A9F7E0 and setter/interpolation +120 were established in the
  dialogue-camera-comfort handoff. No native image patch or guessed ABI.
  L3+R3/F8 share RequestRecenter; existing 1.5 s cinema override remains intact.
- README describes targeting priority and native leveling.

## Validation
PASS: protected-native /O2 x86 build; candidate profile/flavor guard.
PASS: 22 actual-source aim/level checks, including gaze without a tracked hand,
controller fallback before grip, rejection contexts, stale ownership, preserving
native yaw, clearing nonzero pitch/roll, expired/one-shot requests and SHA failure.
PASS: 15 actual-source glow scope checks, including both eyes, matrix restoration,
failed setter restoration, exact texture/viewport/held-target guards and caching.
PASS: existing protected-headtracking 117021 checks; protected-gestures 924 checks.
BLOCKED: existing dialogue_camera_test fixture's fixed-address VirtualAlloc failed
(error 487) before hook assertions. Temporary fixture edits reverted; not a pass.
PASS: emulator startup/gameplay, fallback ray publication/native consumer; native
level callback in build5. Build7 logs confirm corrected eye=0 AND eye=1.
PASS: final build7 captures with head translations +/-0.3m and yaw +/-0.35 rad;
glow follows tentacle heads rather than remaining displaced on the world surface.
NOT RUN: real headset comfort, actual tracked-gaze targeting, nonzero native tilt
correction inside a live dialogue (nonzero correction covered by source fixture).
No FPS improvement claim; resolution and SS125 preserved.

## Artifacts and restoration
Private lab: test-output/tentacle-aim-light-recenter-20261104 (not published).
Contains payload/saves manifests, exact backups, build/runtime logs and captures:
build7-neutral.png, build7-moved.png, build7-opposite.png, runtime-build7.log.
All four original save/config files restored and SHA256 verified after emulator.
Steam accepted DLL and diagnostic INI restored exactly before packaging.
EXE, loader, main INI verified unchanged throughout.
Frozen candidate: build/tentacle-aim-light-recenter-20261104/Install.ps1.
Candidate DLL: B32E1FFF80F933EA606980B863A97FCACC45068EB1938EC532F2E3F11A1E0F30.
Candidate uses accepted diagnostic INI (simulator OFF; collectors OFF) and retains
exact accepted baseline/rollback. No game assets or saves are committed.

## Next gate
Install the frozen combined candidate with game closed; verify receipt/protected
identities. User headset acceptance remains necessary before updating BASELINE
or integrating this branch into main.
