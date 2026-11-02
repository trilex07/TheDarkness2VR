# Handoff: NPC dialogue camera comfort

**Date**: 2026-11-02
**Task ID**: dialogue-camera-comfort-20261102
**Role**: MAIN
**Branch**: codex/dialogue-camera-comfort-20261102
**Worktree**: C:/Users/Trilex/Darkness2VR/.main-camera-shake
**Baseline**: main/origin/main 897c5c44; accepted installed DLL3F575CB4, source c1ac5b8d
**Integration commit**: pending physical dialogue acceptance

## Goal and status

User accepted cinematic theater/controller retirement and requested main; completed
fast-forward and push to897c5c44. New separate request: NPC dialogue must not
force camera downward or leave that angle after dialogue. L3+R3 currently only
recenters HMD, not native camera pitch. Candidate built; headset NOT RUN.

## Context loaded

Bootstrap, BASELINE, WORKFLOW, relevant FAILED-APPROACHES and cinematic handoff.
Preserve accepted no-shake producer, model +100 snapshot, event rumble,
cinema/body ownership, font/texture/audio installation.

## Evidence

Stock EXE SHA256 D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1.
Recenter log 2026-11-02T08:27:18.596Z frame6733: native camera forward
(0.859746,-0.499998,-0.104109), native avatar up(0,1,0), forward
(0.992748,0,-0.120214). Camera/model pitch is -30 degrees while avatar is upright.
Later cinema begins frame6869; dialogue is not proven to be RegionMgr cinematic.

Static verified producer path:
- ConversationViewControlNode registration EB62B0 -> metadata10FC958.
- Its property-copy method989B40 checks metadata10FC958; pointer in native table
  FE1628 at offset9C (FE16C4). Same table offsetAC -> AF78F0 update.
- AF78F0..AF7CF2 interpolates node rotation +150/+130 and calls avatar virtual
  +234 at AF7A29 (return AF7A2B) or AF7AF9 (return AF7AFB).
- Avatar1001630+234 ->9F2770. Getter +2F4 ->C7C710 returns
  **(avatar+62C), the already verified local cameraFFC7C0.
- 9F2770 pushes ESI, pushes input angle pointer, calls camera+8C at9F278C
  (return9F278E). Thus setter entry stack[3] is the avatar wrapper's return.
- FFC7C0+84 ->A9F7E0 returns camera+110 angles. +8C ->9D9120 writes those
  angles and corresponding +120 interpolation state. 7258D1 reads avatar+24C
  ->A450B0 ->camera+84, composing these into owner200/204/208 and native view.
- AF78F0 SHA256 (403h bytes): d10d49e8c6fcdb00d221498649d19e57d5ecf4cb45038a933e58d68e799db46b
- 9F2770 SHA256 (42h bytes):59c42e1eb0d7939200e5e029ddd13cd99c85230d481751af7797cf7a50a338e7
- 9D9120 SHA256 (9Ah bytes):9fc2b97094c608861c2672f4c73aa517fa70438f4e353b8d73ff98f8d3945a58

Do not patch mFacingPitch: its descriptor is the relative angle allowed for
conversation prompts, not view control. 61F010 is debug drawing, not a producer.

## Candidate

Render/DialogueCameraComfort defaults OFF. Opt-in extends the existing immutable
71-method camera table with slot35/8C, retaining protected left getter slot39/9C
and every other method/header. No avatar table changes and no EXE writes.
Suppress setter only on current selected local camera, with valid bidirectional
owner, exact direct9F278E and outerAF7A2B/AF7AFB chain. Forward everything else.
No stored/frozen pose, post-frame correction, native position or body suppression.
This blocks future conversation rotation; does not zero existing saved native
pitch or add a new L3+R3 write path. Runtime may expose an additional producer.

## Validation

- Protected diagnostic DLL build PASS.
- Actual compiled x86 hook PASS against exact native wrapper instruction bytes,
  both synthetic mapped conversation return addresses, unrelated/direct calls,
  disabled setting, stale camera and invalid owner.
- Immutable paired-slot table bounds/header/all other71 methods PASS.
- Cinematic policy/crop/Bink reader PASS; actual x86 cinematic hand retirement
  PASS; protected head117021 checks PASS; stereo transfer order7 tests PASS.
- Physical conversation / exit / stick turn / recenter / cinema regression NOT RUN.

## Deployment and next gate

MAIN only, closed game, exact previous receipt preflight:
tools/deploy-cinematic-theater.ps1 -Install -DialogueCameraComfort -PreviousPackage
C:/Users/Trilex/AppData/Local/Darkness2VR/packages/cinematic-theater-20261102T085451951
Installer backs up DLL and exact game INI, preserves EXE/diagnostic/loader/appdata
INI hashes, enables only Render/DialogueCameraComfort in addition to retained
CinematicTheater=1. Receipt records candidate source and identity; rollback uses
the same tool -Restore <new-package>. No game launch/input injection.

User gate: repeat Johnny conversation with head level, then leave dialogue and
turn using stick. Verify no forced down/pitch/roll and no persistent tilt; inspect
[DialogueCamera] blocked lines. Also preserve cinema transitions and hands.
Do not merge this candidate to main before user acceptance.

Installed source755270d0, DLL SHA256
E03CB97E0E99D753E52DF5440465A66949D2AB9D0DE4700C4F2FCB277C3E8DBE.
Receipt C:/Users/Trilex/AppData/Local/Darkness2VR/packages/cinematic-theater-20261102T094859859.
Independent audit PASS2 backups/2 installed/4 protected; only game INI addition
DialogueCameraComfort=1. ProtectedLeftAim=1 remains enabled in diagnostic INI.
Game closed throughout installation/audit; not launched. Headset NOT RUN.
