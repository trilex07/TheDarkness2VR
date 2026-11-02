# Handoff: Automatic cinematic theater

**Date**: 2026-11-02
**Task ID**: cinematic-theater-20261102
**Role**: MAIN
**Branch**: `codex/cinematic-theater-20261102`
**Worktree**: `C:/Users/Trilex/Darkness2VR/.main-camera-shake`
**Baseline**: main `c8015216`; accepted installed DLL `9AF9A51E7E517621C1C667E192D17B158C2F4DBCE1256F98AA9DEFD79C441A5D`, source `eda9420e`. Accepted fonts, localization and textures retained.
**Integration commit**: pending user headset acceptance

## Goal and status

User requests automatic cinema when the game takes the camera, with genuine
stereo for rendered cutscenes, and cinema for intros/Bink video. Candidate built
and offline checked; physical acceptance NOT RUN. Do not merge to main yet.

Context loaded: BASELINE, WORKFLOW, relevant FAILED-APPROACHES entries, existing
native owner/cinematic admission and Singularity reference implementation.

## Reference audit

Read-only reference: `C:/Users/Trilex/vr-mods/SingularityVR-centurion-runtime`,
commit `c696f864`, `spikes/view_matrix/d3d9.cpp`, `native_cinematic_state.h`,
`cinema_world_screen.h`, `video_cinema_placement.h`.
Singularity uses observed SetCinematicMode events plus authoritative local
controller reflection; native ON is assert-only because some scripts leave the
bit clear. Its screen is latched in room coordinates, not continuously attached
to the HMD. These UE3 offsets/events cannot be copied into Evolution Engine.

## Darkness native evidence

- Existing accepted admission pins stock EXE SHA `D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1`, RegionMgr vtable `FC7D00`, slot `+1AC` at `FC7EAC` -> `6CF490`, bytes `8D 81 68 02 00 00 C3`. Getter returns smart reference at `manager+268`; only reference nullness is read in scoped native view callback.
- `native_player_camera_owner.h` and `native_view_boundary_runtime.inl` already validate this contract. No new guessed native offsets or image writes.
- Installed EXE delay imports `binkw32.dll`: `_BinkWait@4`, `_BinkClose@4` verified by pefile, not ordinary imports. File is `Darkness II/Tools/binkw32.dll`, SHA `1A8E98434AFD92412ADD96DC7C24885C88067F368A21C6A28445EF049FF09FE9`. Export RVAs Wait `123E0`, Close `11EC0`.
- External codec hooks are admitted only for that hash after native Present readiness. Both modules pinned; EXE/IAT untouched. Observe native playback polling, not Open (preloads are not proof of visible playback). Multiple handles tracked, Close releases each handle, no arbitrary timeout or game-memory writes.

## Candidate behavior

- `[Render] CinematicTheater=0` default. Protected diagnostic build and SubmitMode4/Stereo1 required. Installer enables only this key in current game INI.
- Current-frame native cinematic state selects two eye-specific world-space XR quads. Both completed eye images retained, crop to common angular rectangle, keep native stereo separation. HMD head delta removed before world draws in native view callback; both live/replay params refreshed together. Controller weapon/aim/throw placement retires in that state, native cinematic hands path retained.
- Bink playback selects mono image on both eye screens, preserving source-frame aspect. No invented depth for prerecorded movies.
- Screen anchor latches level in LOCAL room space on entry; distance/width use existing CinemaDistanceM/CinemaWidthM settings (existing defaults 2m/2.1m). XR reset/recenter/reference-space change clears anchor. Returning to gameplay resets head reference.
- Logs `[CinematicTheater]` on transitions and `[CinemaBink]` on hook admission/refusal.

## Validation and limits

- x86 `build-cinematic-theater.ps1`: mode/UNKNOWN rejection, Bink handle lifecycle (duplicates/multiple handles/capacity), binocular angular-crop math, actual native cinematic reader with invalid/absent references: PASS.
- Existing protected-head regression: PASS 117021 checks.
- Stereo transfer ordering suite: PASS 7 tests.
- Protected diagnostic production DLL build: PASS. This is offline evidence only.
- Need actual headset proof: native cutscene entry and exit, film intro/Bink exit or skip, readable subtitles, room-fixed screen when looking away/leaning, no regression to hands/tentacles/weapons/haptics afterward.
- GetPlayingCinematic is a native cinematic signal, not yet proof that EVERY loss of camera control uses it. Scripted restraints/finishers outside this signal require event-correlated evidence before extending detection. Missing native observation does not reuse stale ON.
- Native renderer still widens the cutscene FOV using accepted cover; framing/size are a headset gate. No theater room mesh, fake stereo, or generated film frames added.
- External Bink Wait means codec playback polling; native background usage or a retained handle after playback needs runtime investigation if observed. No claim all video paths covered without a test.

## Deployment

`tools/deploy-cinematic-theater.ps1 -Install` requires game closed, exact accepted
DLL/stock EXE, committed source, protected build. Copies candidate and backups
to `%LOCALAPPDATA%/Darkness2VR/packages/cinematic-theater-*`; changes only DLL
and one game INI key; preserves diagnostic/local INI, EXE, loader. Failure
restores both backups. Receipt records source/hash/preimages/after images.

Closed-game rollback: `tools/deploy-cinematic-theater.ps1 -Restore <package>`.
Refuses modified candidate files; exact accepted preimages restored. No Steam
launch or injected input is part of installation. New font/audio/texture archives
are outside this installer.

### Installed candidate receipt

Source `f247d108`, DLL SHA256
`3126053204B354D99B0CB0D8BEBC8A0178C2031E5D5C34CA7CC3F17BA46F4374`.
Package `C:/Users/Trilex/AppData/Local/Darkness2VR/packages/cinematic-theater-20261102T084535813`.
Independent audit:2 backups,2 installed hashes,4 protected hashes PASS;
configuration semantic diff contains only `Render/CinematicTheater=1`.
Game closed at installation and audit. Steam/headset run NOT RUN.
