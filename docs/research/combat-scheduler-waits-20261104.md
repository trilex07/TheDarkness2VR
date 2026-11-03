# Retained combat scheduling evidence

This extends combat-critical-path-search-20261103.md with actual CSwitch and
ReadyThread events from the SAME retained ETL. The previous JSON sample export
omitted these arrays; the original ETL still contains them. Do not infer their
absence from empty exporter fields. No game launch, deployment or runtime code
change is part of this follow-up.

Source: capture20261030T221816Z-61b77f88, PID26912, render TID7828,
manifest valid=true, etwLostEvents=0, errors empty. Historical DLL0B59770A;
not the current accepted E56BF027 or new combined candidate.

## Two complete frame intervals

PresentMon absolute QPC milliseconds were converted to ETW microseconds with
origin544101883487/frequency10000000. Scheduler intervals are clipped to those
same boundaries, not compared to an unrelated aggregate. Both intervals have
complete RUN plus off-CPU coverage in the padded exports.

- Frame28964853.1..29002951.2us, 38.0981ms: RUN19.3751ms, off-CPU18.723ms.
  The largest sleep is28981573..28997245, 15.672ms. ReadyThread28997236,
  scheduled9us later. Ready context: own process TID32128.
  Exact saved DLL/PDB resolves resume stack28997245 through NVIDIA D3D9 and
  system D3D9 into proxy CaptureBackbufferToShared -> StartStereoTransfer ->
  RenderEyePairFromReplay -> VrFrame -> ApplyBoundaryDecision.
- Frame11003744.4..11034656.1us, 30.9117ms: RUN15.1657ms, off-CPU15.746ms.
  The largest sleep is11008151..11020643, 12.492ms. ReadyThread11020635,
  scheduled8us later. Ready context: steam.exe/TID6660. Resume stack11020643
  goes through tier0_s/steamclient -> native4F1DE5 -> native8419E2.
  Exact EXE decompilation identifies function4F1BF0 calling SteamNetworking()
  and vtable+4 at4F1DE3, returning4F1DE5. This is the packet-availability
  poll, not the separate SteamUser()->vtable+8 call returning4F1D27.
  Caching SteamID would target the wrong wait and is not an admitted fix.

Kernel UserRequest is a wait category, not a named lock. ReadyThread records
the signaling execution context; it does not by itself prove lock ownership or
an IPC peer. Here the separately captured user stack and native call-site
evidence establish the API paths. Neither wait is mainly delay awaiting CPU
assignment: scheduling after readiness was8/9us.

## Limits and implementation boundary

The historical transfer route predates accepted transfer/isolated-output
optimizations. Its15.672ms cannot be claimed as current removable cost or as a
measured benefit of the new package. Native networking has packet/connection
side effects; do not throttle it, cache availability or move it to a worker
without proving native ownership, multiplayer context and publication ordering.
No Steam API, EXE bytes, network behavior or required GPU fence was patched.

Current simulator/capture2 evidence is separate. In capture2's10..11s slice,
render49316 RUN876714us, off120183us, first/last boundary outside coverage.
Observed paired readiness delay13731us over the second; missing readiness is
kept unknown. This suggests investigating actual owner work before changing
thread priority/affinity, but is a villa slice, not combat or HMD FPS proof.

The user25ms/40FPS cause and recovery remain unestablished. This follow-up
narrows historical long-frame paths; it does not resolve current performance.

## Reproduction and evidence identities

xperf dumper with provider3d6fa8d1-fe05-11d0-9dda-00c04fd7ba7c exports
CSwitch/ReadyThread. Padded ranges28900000..29100000 and10950000..11100000us.
Separate all-provider -stacktimeshifting exports at the resume timestamps
retain stacks. Resolve proxy symbols using only that capture's pinned DLL/PDB
and a unique local symcache, never the latest build's symbols.

tools/perf/analyze_scheduler_frames.py accepts xperf CSV, explicit frame bounds,
PID/TID and output. Tests exercise clipped RUN/blocked/ready separation, foreign
PID rejection, missing coverage/readiness and truncated row rejection. Three
fixtures PASS; both real frame intervals report complete coverage. The parser
does not silently certify upstream ETL loss/completeness.

Private exports remain in the task test-output directory, not Git:

- historical-scheduler-worst.csv SHA2567cf5c331f947c718f3aae9713242f6614e459255f3409e3819a595a2c4279b77
- historical-scheduler-31ms.csv SHA2561e53828250f16671af2a990b3af9eab2db4278c95ae6d61c205a3b9adf1e2b8b
- historical-31ms-resume-stack.csv SHA2569e8cc0a4be552d08311e5efa16e904e0d972051d37ce2046dd389c36028a0a9f
- historical-38ms-resolved-stack.csv SHA256a6960217fc5f9867ff46ac119bf1c22bab44d3e482651d3e5017c8a6f164219c
- private combined-steam-wait-evidence.txt SHA2563e40edbbfdf7c7ba5f58d25a6b5fea2fe45ef6aa04f5951fb476d6a33b520f5b

Ghidra queried the existing exact-build project read-only. Its default OSGi
cache failed initially; isolated application.settingsdir succeeded without
altering the user's settings. No native decompiled source is published here.
