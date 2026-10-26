# Combat profiling attempt: crash, not accepted (2026-10-26)

The user confirmed the game exited without their action. No further launch is
authorized by this attempt. DLL and both INIs were restored and hash-checked.
Private evidence is in LocalAppData/Darkness2VR/perf/20261026-093553; do not
commit its ETL, logs, or executable data.

## Verified observations

- PID 6712, render/Present TID 20756. Source dimensions logged as 2048x2091.
- WPR reports zero lost events and buffers; the queried PresentMon session
  reports zero lost events. PresentMon contains 2852 game rows.
- Re-reading the exact QPC phase boundaries gives 534 baseline and 2208 combat
  rows. Baseline FrameTime mean/p95: 15.239/18.343 ms. Combat: 15.851/19.432 ms,
  p99 30.324 ms, worst 69.202 ms. These are game Present intervals, NOT HMD FPS.
- Combat GPUBusy mean/p95: 3.511/4.187 ms; GPUWait mean: 11.857 ms. This does
  not support treating the entire GPU timestamp interval as GPU execution.
  PresentMon CPUBusy is not proof of CPU execution or a specific wait stack.
- Whole-trace sampled module weights are highest in nvd3dum.dll, ntoskrnl.exe,
  and the game EXE. They include rundown/post-capture time and cannot isolate
  the critical path. The all-event stack butterfly mixes sampled and scheduling
  events; its hit percentages MUST NOT be reported as CPU time percentages.
- The final exception is C0000005 at EXE VA 00E129EA, reading unmapped
  471AA1A2 on render thread 20756. EBP chain begins 004E0E9A, 00D163C1,
  00E37C39, 00D174FB, 00897886, 00705FF9. This is the previously documented
  buffer-copy failure location, not proof of the same corruption producer.
  Do not attribute the fault to profiling, hands, or throwing without evidence.

## Corrections made

The original manifest remains invalid and is preserved. Its parser expected
CPUStartTime rather than PresentMon 2.6 CPUStartQPCTime, and missed xperf's
`Total # Lost Events` label. It also checked game lifetime after trace saving
rather than at the requested capture end. These three collector defects are
corrected for future captures; they do not turn this crashed trial into PASS.

The Present observer also wrote each repeated success status synchronously via
Log (open/flush/close under a lock). The final counters contain 7000 statuses.
Dispatch now counts every result but reports repeated identical success statuses
only at the existing 1000-entry cadence. First return, transitions (including
recovery to OK), every failure, and latched faults are immediate. No forwarding,
HRESULT, fence, render setting, or native ownership behavior changes.

Regression: 7000 identical statuses emit 10 records instead of 7000, with all
7000 returns counted. Recovery, different status, and failure are tested.
This is a bounded diagnostic I/O correction, NOT evidence of improved game FPS
and NOT a crash fix. It is OFF in builds without the diagnostic observer.

## Status and next gate

IMPLEMENTED / OFFLINE VERIFIED: collector corrections and redundant status I/O
reduction. Python analyzer (2 tests), Present readiness (8 tests), x86 actual
dispatch/rollback/recursion regression, diagnostic DLL build: PASS.
IN-GAME VERIFIED / HMD ACCEPTED: NOT RUN for this candidate.

Remaining: isolate render-thread sampled execution versus scheduler waits during
the requested combat window; trace the invalid copy source back to its producer.
Do not disable synchronization based on GPUWait alone. No claim of 90 FPS or
completed optimization. A further launch requires authorization after the crash;
offline analysis can continue from the retained trace.
