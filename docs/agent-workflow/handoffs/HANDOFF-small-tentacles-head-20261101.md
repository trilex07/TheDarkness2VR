# Handoff: four small tentacles follow head rotation

**Date**: 2026-11-01
**Task ID**: small-tentacles-head-20261101
**Role**: MAIN
**Agent**: Codex
**Branch**: codex/gaze-nis-main-integration
**Worktree**: C:/Users/Trilex/.codex/worktrees/gaze-nis-main-integration/Darkness2VR
**Baseline**: restored pre-DLAA installed DLL `CC7DD6C7B048158255D607649841BDE6305D284414CAD790A4CE99B14A941248`, source `b9b1d190`; stock EXE `D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1`. DLAA headset trial rejected and fully rolled back.
**Integration commit**: `88ea3095`

## Goal

Make the four smaller headless player tentacles follow physical head rotation in the existing opt-in head-attachment path, without attaching enemies, hands or held targets.

## Status

Protected candidate installed with complete rollback. User reports four small tentacles now follow head correctly. Same run exposed a separate held-enemy finisher input issue, tracked in `HANDOFF-held-finisher-a-20261101.md`.

## Evidence

- Existing `TentacleHeadScope` covered only the 302920-byte VB/213588-byte IB player tentacle mesh. The current runtime log confirms `mesh-scoped` only for that resource pair; the log is capped at four successful scope entries.
- A retained complete same-frame capture `Darkness2VR-matched-33700-2-108226` contains four consecutive eye-0 draws 7..10, all shader `1CB7CEE2`, stride 40, stream offset 0, same 53120-byte VB/11520-byte IB and texture, with four distinct skin banks. The frame preview shows four small headless strands around the lower field of view. Both resource sizes and shader identity are excluded by the previous scope.
- Those four draws have a camera-local c3.z/w of approximately `(-0.2,-0.1)` versus `(-0.1,0)` for the large tentacle mesh. This independently fails the prior secondary matrix gate. The separate mesh, repeated count and visual scene support attribution to the four small strands; a fresh instrumented headset draw witness is still needed for final identity/behavior confirmation.
- The change admits only this complete resource/shader/matrix signature under the existing `HeadAttachedTentacles=1`, SubmitMode 4, stereo, head-geometry, non-shadow and fresh native Empty/no-held-target gates. No native physics or bone palette is changed.
- Extracted actual-source scope fixture: 27,099 checks PASS, including both eyes, all four repeated draws, the shared native-camera head transform, held-target refusal, foreign shader/size/offset/matrix refusal and matrix restoration. Protected-native x86 DLL build PASS; profile gate PASS. Candidate SHA256 `EFDCCFB0B40F37F716014DF836F5BE127121177B5C88EABC3E3714B828375B9C`.
- Ordinary x86 build succeeded only outside the restricted sandbox because local MSVC linker reported MSPDB140 version mismatch inside it. Standard build is ineligible for the current protected profile; protected build succeeded outside the sandbox.

## Next Steps

1. User checks idle head yaw/pitch for the four small strands in both eyes, then a native grab/hold/release and enemy/world stability. Read `HeadAttachedTentacles` log on that exact candidate.
2. If the four strands still fail to follow, capture their current draw signatures rather than broadening the gate by shader alone. Restore the complete package if image or interaction regresses.

## Installed MAIN receipt

- Package: `C:/Users/Trilex/AppData/Local/Darkness2VR/packages/small-tentacles-head-20261101T133218114`.
- Installed protected-native DLL: `EFDCCFB0B40F37F716014DF836F5BE127121177B5C88EABC3E3714B828375B9C`; package source commit `88ea30953e134ba7bb15dda3b3b9f5a6ad0fb6a7`.
- Independent verification: nine backup hashes correct, 9/9 installed protected targets correct (only DLL changed), stock EXE correct, DLSS host directory absent, game closed and not launched.
- Runtime log on installed candidate confirms four `mesh-scoped` draws with VS `1CB7CEE2`, VB `53120`, IB `11520`, offset `0`, eye 0. User reports the four small tentacles behave correctly in headset. Per-eye exhaustive and held-target behavior were not separately reported.

## Blockers

User-confirmed head-follow behavior; exact both-eye/held-target acceptance is not separately recorded.
