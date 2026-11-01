# Handoff: native CPU submission gate

**Date**: 2026-11-01
**Task ID**: native-cpu-gate-20261101
**Role**: MAIN
**Branch**: codex/gaze-nis-main-integration
**Baseline**: installed gaze NIS DLL 916B9A51; remote main 57160de3.

## Goal

Make the installed native float/binding CPU filters eligible when software
vertex processing is the only denial source, and identify any remaining
fail-closed cause without changing the renderer's visible output.

## Status

Source and offline tests complete. Installed in Steam; runtime CPU benefit and
admission pending user headset test.

## Changes

- A software-processing call invalidates the entire submission epoch around
  the native call and pauses reuse during callbacks. It no longer permanently
  denies the filter. Hook failure and every other unsafe mutation still deny.
- Sparse status includes denial-source bits and first software-call result.
- No gaze LOD, EXE patch, image quality, stereo or input change.

## Evidence

- Current installed log: requested=1, enabled=0, denied=0x10, all filter
  skipped counts zero.
- Native submission boundaries: unknown alias, callback and software modes
  PASS; epoch and native float-cache tests PASS; status WARP test PASS.
- Protected x86 DLL and profile gate PASS. Normal PDB link failed due
  MSPDB140.DLL mismatch; isolated no-PDB build with MAP succeeded.
- Candidate DLL SHA256 F5BEFA82966F8A02CAE04B9B30C40BE86514D72F95399E452F9FD5C6FCB577FE.
- Installed package:
  C:/Users/Trilex/AppData/Local/Darkness2VR/packages/native-cpu-gate-20261101T025125859.
  Postinstall DLL and stock EXE hashes match; all nine rollback backups match
  their recorded hashes; both active INIs retain native filters and gaze NIS.
  Game was not launched by the agent.

## Next Steps

1. User runs the problematic scene. Read NativeSubmissionGate, NativeSubmission
   and NativeBinding counters; compare actual application frame timing.
2. If denied, investigate the recorded source before widening admission. If
   enabled with skips, inspect both eyes, weapons, menus, Reset and stability.

## Blockers

No current worst-scene CPU trace. Actual skipped call count and FPS gain are
unknown. Native engine LOD remains unimplemented.
