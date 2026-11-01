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

## First headset run

The user ran the installed F5BEFA82 DLL. The new log reports
`[NativeSubmissionGate] sources=0x1 softwareTransitions=0`, with
`enabled=0 denied=0x10` and zero skipped native float/binding calls.
The earlier software-processing fix therefore saved zero CPU in this scene.
World-bearing XR cadence was near 90/s; it does not establish engine FPS or
solve reported worst-scene drops.

The next candidate leaves a paused replay/driver callback as an epoch
invalidation, retaining permanent denial for unpaused callbacks. It records
the first unpaused callback context: bit 1 nativeDepth, bit 2 replaying,
bit 4 nativeCalls, bit 8 neither. A broad bypass around every DriverScope
was rejected during review because AddRef occurs on resource retention and
would invalidate cache entries at nearly every binding.

The second protected x86 DLL is 63C72AE1A4253ED448A71742E5073A410B6600769C5846F5DAEE54E64B151630.
Native boundary and recorder tests passed; the protected profile gate passed.
Install/test through `tools/deploy-native-cpu-callback.ps1`; neither admission
nor saving is established until the next headset run.
