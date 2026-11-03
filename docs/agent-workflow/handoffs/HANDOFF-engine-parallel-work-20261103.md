# Handoff: larger full-packet worker

**Date**: 2026-11-03
**Task ID**: engine-parallel-work-20261103
**Role**: MAIN
**Branch**: codex/engine-parallel-work-20261103
**Worktree**: C:/Users/Trilex/Darkness2VR/.main-camera-shake
**Baseline**: parent ec5d6875; installed accepted DLL B97B4477, cinematic-theater-20261102T193328001
**Integration commit**: pending acceptance; no merge or deployment

## Goal and status

User requests substantial actual offload, preserving resolution and accepted
stereo/hands/tentacles. Full VS packet worker implemented and offline validated,
default OFF. Benefit remains unproven and the realistic-sized synthetic CPU
work is only~0.19ms. Do not describe correct threading as a large FPS win.

## Context loaded

BASELINE, workflow, failed approaches, performance plan/audit/candidate and
native submission/ownership evidence. Details and all gates:
`docs/research/full-packet-worker-20261103.md`.

## Changes and evidence

New pure packet core, bounded owned worker and actual replay integration;
independent startup switch FullPacketPrepareWorker. Tests compile the existing
serial implementation verbatim as oracle.942908/942909 packet/lifecycle checks PASS;
production+diagnostic mixed consumer tests each2.3million records PASS;
protected-native build, head,47/130 hands and five-mode installer PASS.
Private build/packet-*.log. Exact engine job-pool and resource-class findings
in research doc; native exports remain TEMP, not Git. No new native patch.

## Next steps

1. Freeze exact source/DLL/INI package with DefaultMode Packets; record receipt.
2. MAIN may deploy only in an authorized test window with game closed. No
   launch/install was requested in this turn. Keep accepted Steam files intact.
3. Use Packets versus Off with identical DLL/settings to measure total costs
   and hits/fallback/join. Headset correctness and combat FPS are NOT RUN.
4. The CPU ceiling is small; if runtime is neutral/negative, retain OFF rather
   than treating thread count as progress. Larger native work still needs a
   proven producer/lifetime/dependency boundary, not a guessed GraphicsWorker
   setting or an unprotected D3D9 device migration.

## Notes

Never stage build/ or test-output/ wholesale. Old shader-cache package remains
frozen and separate. Installed DLL/INI/protected receipt hashes must still match
cinematic-theater baseline; Git branch does not identify deployed binary.
