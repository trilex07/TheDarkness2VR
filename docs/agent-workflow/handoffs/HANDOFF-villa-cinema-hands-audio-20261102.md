# Handoff: villa cinema, peaceful hands and natural Polish dialogue

**Date**: 2026-11-02
**Task ID**: villa-cinema-hands-audio-20261102
**Role**: MAIN
**Branch**: codex/villa-cinema-hands-audio-20261102
**Worktree**: C:/Users/Trilex/Darkness2VR/.main-camera-shake
**Baseline**: accepted main897c5c44; installed dialogue source755270d0,
DLL E03CB97E0E99D753E52DF5440465A66949D2AB9D0DE4700C4F2FCB277C3E8DBE.
**Integration commit**: pending user headset acceptance; local candidate only.

## Goal / status

Candidate for villa false cinema and stiff peaceful hands; six revised lines
in S03MansionI boss-crazy conversation. Elevator line not identified yet.
Read BASELINE, WORKFLOW, FAILED-APPROACHES and dialogue/cinema handoffs.

## Evidence and changes

Stock EXE SHA256 D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1.
RegionMgr+268 nonnull means a running sequence, not necessarily camera takeover.
Descriptor10A9174 mFreeCam says 'Do not take control of player\'s camera'.
480148 pushes descriptor,480156 LEA object+7BC; descriptor10A9168 mOnRails
at480169/480177 maps+7BD. Serializer480140 length42 SHA256
b3150c8e065076d6dd8c11671ba3bc2f776e70f4c4f6796106c81fddd5a15edb.
803F43 tests FreeCam;812BB3/812BC0 skips player camera acquisition if set.
Actual class tables FDD0B0 (constructor C7030A), FDD318 (8587DD/CB375F).
Both +108 point803F30; admitted together with stock EXE/serializer digest.
Read flags only during scoped native callback. Unknown class/invalid flags fail
closed. Same-frame camera-taking signal now requires running && !freeCamera.
Bounded transition log [CinematicView] exposes object/table/flags/takesCamera.
No frozen pose or native camera write introduced; Bink path unchanged.

Protected human-palette application no longer rejects Unknown demon grab state:
peaceful scenes can lack demon equipment. Legacy weapon route retains its grab
requirements; selected resource, bidirectional owner, palette and bones checks
remain. Actual production grab predicate is extracted into x86 regression.

Six lines Sd02690Wiseguy1,02700Wiseguy2,02710Wiseguy1,02720Jackie,
02730Wiseguy1,02740Wiseguy2 shortened and regenerated using original voice donors.
Private report test-output/dialogue-timing/natural/report.json records native
budgets, actual durations, quiet tail and tempo=1.0. No speech time stretching.
30ms lead/80ms tail preserved; generated quiet tail required. Only silence padded
to native budgets. xWMA codec/header/phoneme invariants and decode counts checked.
Installed Languages.cl changes only these six subtitle keys. Cache writer copies
existing archive, appends replacements, restricts TOC edits to replaced entries.
Eight files H.Misc + H/B/F.Misc_en cache/toc; protects current font/texture set.
Does NOT reinstall full Polish pack. Natural packer now rejects overlong speech.

Private broad audit:1412 native matched lines,706 raw voices exceed 1.15x native
budget. This is a backlog, not proof all installed lines run at that exact speed.
No claim global dubbing fixed. Elevator dialogue remains unresolved.

## Validation

- Protected diagnostic DLL build PASS, SHA256
  FD6470F6B135A49DD77E30A023A5A931683025C719455EC3EAAA7220A01443AF.
- Cinema policy/crop/Bink/native FreeCam/OnRails/unknown-class tests PASS.
- Actual x86 hands pack/retirement and peaceful Unknown grab predicate PASS.
- Protected head117021 checks PASS.
- Existing dialogue fixture BLOCKED this run: VirtualAlloc fixed native address
  mapping fails before hook execution, including elevated and fixed-base trials.
  Trials reverted; production dialogue hook untouched. Previous runtime log
  [DialogueCamera] blocked AF7A2B confirms accepted previous guard was active.
- Six native audio encode/decode roundtrips PASS; final ASR evidence retained
  privately. Short ambiguous takes regenerated rather than accelerated.
- Physical villa/no-cinema, peaceful hands, true cinema/Bink and dubbing NOT RUN.

## Deployment / next gate

Use committed source and tools/deploy-cinematic-theater.ps1 -Install with previous
receipt cinematic-theater-20261102T094859859; game closed and exact preimages.
Then tools/deploy-asset-repair.py install --package <private natural package>
--backup <fresh path> --game <Steam game>. Both support exact rollback.
Update installed receipts below after installation. Never infer runtime success
from offline tests. Test villa free walking/hands, real cutscene and boss-crazy
conversation; identify elevator line using a specific phrase if still rushed.

## Installed candidate (MAIN, game closed, no launch)

Source44d9e7baff15aea1b58d8678b90cdf2fe981ff42. DLL FD6470F6B135A49DD77E30A023A5A931683025C719455EC3EAAA7220A01443AF.
Runtime receipt:
C:/Users/Trilex/AppData/Local/Darkness2VR/packages/cinematic-theater-20261102T170623530/deployment.json.
Audio receipt:
C:/Users/Trilex/AppData/Local/Darkness2VR/packages/villa-natural-dialogue-20261102T1707/deployment.json.
Private audio package remains test-output/dialogue-timing/package-final;
manifest SHA recorded in receipt. New short takes have ASR evidence; final
decoded ASR matches A nie/Co/Dobrze wygladasz/Czego trzeba, near matches other
two (Czefowi/Oszalau). ASR is supporting evidence, not human listening acceptance.
Independent post-install audit PASS: DLL2 backups/2 installed/4 protected;
audio8 backups/8 installed/19 protected including all current font/texture files.
No source main integration, game launch or headset acceptance performed.

Rollback, game closed: deploy-asset-repair.py rollback --backup <audio receipt
directory> --game <Steam game>, then deploy-cinematic-theater.ps1 -Restore
<runtime receipt directory>. Audio restoration retains current DLL; runtime
restore returns previous dialogue candidate E03CB97E and exact previous game INI.

## User rejection and live read-only diagnosis (2026-11-02 follow-up)

User: villa theater still activates; hands still rigid. Do NOT merge candidate.
User identifies elevator line as 'co tylko bedziesz potrzebowal, Dzeki': strong
manifest match Sd02170Vinnie, EN 'Yeah. Sure. Whatever you need, Jackie.'
SIDE prompt: docs/agent-workflow/TASKS/PROMPT-SIDE-polish-dialogue-audit-20261102.md.
No SIDE agent launched; user requested a prompt.

Process35172, loaded proxy54A50000; installed DLL hash matches FD6470F6.
Read-only snapshots via tools/observe-villa-state.py; no injection/input/write.
Private files test-output/villa-live-20261102/{state,owner-state,tracking-state,
bink-state}.json and copied d3d9.log. Asynchronous samples are not ordering proof.
Log switches to mode2 at frame4322 native0/video1. FreeCam native detector stays
running0, so this false positive is the separate Bink path, not native cinematic.
Two sticky Bink handles47A6DA00 and01A96040 remain in registry. Last headers
start512/256; frame count/current (offsets8/C, consumed at4782AD/B0) 480/327
and450/301. Width/height interpretation is suggestive, fullscreen ownership not
proved. Do NOT fix with arbitrary video dimensions, timer expiry or disabling
all video when a world camera exists. Need native movie-presentation ownership.
Static Wait callsites478177/478347 both receive object+ D8; CloseCE5EFC clears
that same resource field. Wait alone does not distinguish full-screen playback
from other video users. Import slots Wait10DBF2C, NextFrame10DBF28, Close10DBF04.

Hands: private resource ready1, tracked-hands/input/gesture enabled1, native
cinema0, world1, head geometry/reference valid1, tentacle pause flags0.
Native owner chain valid, main/off slots point to native empty sentinel, demon
slot exists. During fresh gameplay selected_resource2E53B4F0 and native sample
camera27B0FCC0 valid; pose_camera0 and both published poses zero. Other samples
native gameplay clears. Latest controller buffers have flags/valid zero, so
cannot attribute empty poses to one specific admission check yet. Need user
unpause+move controllers capture, requested asynchronously. Publication currently
runs in XInput poll, whereas consumption requires render-thread/frame admission;
timing/thread is a hypothesis, NOT a proven reason or authorization to bypass.
No replacement DLL/assets installed in this follow-up; game left running.
