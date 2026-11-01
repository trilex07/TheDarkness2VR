# Handoff: camera shake / animation stabilizer (Claude, 2026-11-01 21:30)

Branch `codex/gaze-nis-main-integration`. Continues `HANDOFF-shotgun-twohand-shake-20261101.md`; details in `docs/research/darkness2-camera-shake-20261101.md`.

## Installed now (user verdict: "tragedia" - roll back first)
- Package `%LOCALAPPDATA%/Darkness2VR/packages/camera-anim-stabilize-20261101T212812295`, rollback `rollback/Restore.ps1` -> returns the owner-trace baseline DLL `7414984F...` (shotgun works there). Run with the game closed.
- INI `Darkness2VR-diagnostic.ini`: `CameraAnimStabilize=1`, `CameraShakeSourceSuppress=1` (both default OFF in source), `ShotgunCandidate=1`, `CameraShakeTrace=1`.
- Deploy script `tools/deploy-camera-anim-stabilize.ps1 -Install` (commit, build -DiagnosticCapture, 9-target backup; auto-restores prior stabilizer/heap package). Builds are done by the user on Windows.

## What the user sees
Shake is camera motion from first-person animation (run, tentacle swings, hearts) plus hit/explosion/train shake. Wanted: no camera shake at all, controller rumble on hits/train, no rumble on stick turns, weapons steady.

## Tried, with evidence
1. EXE call-site patch of generator 00628420: refused by `BlockNativeImagePatches=1` (FAILED-APPROACHES).
2. Zero shake factors on owner `avatar(01001630)+0x5E0 -> 00FDF170`: owner idle, all fields 0 (FAILED-APPROACHES).
3. `src/camera_anim_stabilizer.inl` (v1-v6): stable camera = avatar parent `+0x1F8C -> 00FFC300 +0x140` heading + averaged offset, applied as `D*H` to the view head delta (BuildEyeParams) and `E*C_anim` to hands/gun/shot (`StabilizedCameraWorld`). v1 removed sprint bob (user OK). Later versions: weapons jitter, eye mismatch. Key log fact: `cameraMovedAfterCompute` == every frame -> the game writes `camera+0x80` AFTER the first BuildEyeParams/hands use, so any correction is one frame late; high-frequency shake cannot be cancelled after the fact this way. v4 also showed BuildEyeParams runs twice per frame (eye 0 before, eye 1 after the selected-owner callback).
4. `src/camera_shake_source.inl` (v7): background scan finds 5 `00FDF170` objects; per frame zero `+8C0/910/960/CA0` factors (+10..+18,+24) and impact sub-system enable bytes `+1170+{0,70,E0,130}+10`. Log: `refills` ~4 per frame -> something rewrites these factors every frame (static analysis said load-only; wrong or another writer). Active owner was `3AAB1580` (not the avatar-linked one). `peakImpulse` always 0 -> no rumble.

## Open leads
- Find who rewrites the 00FDF170 factors per frame (hardware write breakpoint on `owner+0x8D0` of the active owner) and which owner/field actually reaches `camera+0x80`.
- Better: find the native camera composition that adds `+0x1F0/+0x20C` (and the impact output `+1170+0x180/+0x190`) into the camera and suppress there via a heap-owned route, or write the stabilized matrix into the camera before the renderer consumes it (needs proof of order).
- Menu L1/R1 -> LB/RB passthrough added in d3d9.cpp (`[MenuShoulders]`, uses `MenuConfirmActive`); user result for talent trees unknown.
- FPS dips (86-90 at 90 Hz, one 74) not investigated.
