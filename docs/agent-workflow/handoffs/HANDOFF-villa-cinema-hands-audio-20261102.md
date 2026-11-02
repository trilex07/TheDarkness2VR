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
