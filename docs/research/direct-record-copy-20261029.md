# Direct recorder payload copy — 2026-10-29

Status: IMPLEMENTED / OFFLINE VERIFIED. Not run in game or HMD.

## MAIN integration and manual-test scope

The original Opus copy/filter implementation is retained unchanged. MAIN
integrates e491485 on top of 9c831d5, including the inherited right-eye WVP/STP
correction, independent hands/aim, tentacles, input and accepted optimizations.
Configuration differs deliberately: default OFF; only a DiagnosticCapture build
reads `[Render] DirectRecordCopy` from `Darkness2VR-diagnostic.ini`. Production
builds remain OFF. No other renderer or gameplay settings are changed for this
candidate. The unfinished VR settings panel is preserved separately and excluded.

PAGE_GUARD restoration is an attempt, not an ownership guarantee. A fault may
consume a guard before the filter runs; another thread can read/change/unmap
the page in that interval, and VirtualProtect restoration can fail or race with
another protection change. The filter does not establish original protection
ownership or make query/protect atomic. Single-thread guard tests, ordinary
four-thread readable-record tests and a successful game run cannot prove the
absence of these risks. No concurrency-safety claim is made. This is an explicit
opt-in manual runtime candidate with a verified prior DLL/INI rollback, not a
new accepted baseline. On crash/corruption restore those exact files and disable
the diagnostic switch; never launch an automatic second test.

## Indoor → outdoor delta (existing protected trace, render TID 42504)

`protected-outdoor-exclusive-cpu-20261026.csv`, disjoint sampled leaves, ms/frame
outdoor minus indoor: native game +1.05, memory-query leaves +0.97, WOW64 memset
+0.96, syscall dispatch +0.43, proxy C++ +0.42, other kernel +0.38, D3D9 +0.32,
NVIDIA +0.27, other +0.53. Sum 5.33 ms ≈ the 5.35 ms RUN growth. Blocked/ready
grow far less (outdoor 1.99 / 0.28 ms per frame). The WOW64 memset is the
2,080-byte service-context clear executed on every 32-bit syscall, so query,
memset and dispatch scale with syscall count. The WOW64 query subtree grows
1.925 → 3.251 ms; it overlaps RecPush (1.496 → 2.486 ms).

## Avoidable operation

Every recorded payload (`RecPushLocked`) made a fresh `VirtualQuery` before
copying. Outdoor A receipts give ~2,083 VS + ~1,550 PS float uploads per frame.
The opt-in native-owned proof covered only 16% of inspected uploads in game
(1,491,991 / 9,277,452, run a57e131) and is OFF by default; SmallRecordCopy
still issues one RPM syscall per payload. Cost scales with outdoor draw volume.

The prior "SEH-only copy" rejection was that it could consume PAGE_GUARD.
`d2vr_record_copy::DirectCopy` copies under SEH; on STATUS_GUARD_PAGE_VIOLATION
its filter re-arms the faulted page before the legacy fallback runs, so the
fallback sees and refuses the original protection. Any fault falls back to the
unchanged legacy prefix/zero-tail copy and sticky incomplete-packet receipt.
No permission cache, no retained pointer, no lock change, no native patch.
Native-owned proof copies keep precedence. Rollback: `Render/DirectRecordCopy=0`.

## Validation

Actual RecPush/RecEmitData harness, direct OFF and ON: 10,681 checks, including
NOACCESS and guard pages at/inside/crossing the boundary, re-armed guard still
raising for the next reader, null/clamped sizes, 8 MiB, dirty arena reuse,
4-thread atomic records and register-bank receipts; zero RPM calls when ON.
Synthetic x86 (WOW64) RecPush: 64 B 427 → 12 ns, 272 B 426 → 17 ns,
1088 B 456 → 99 ns per call. Both x86 DLL builds and 16 dependent suites PASS.

Expected saving ≈ 3,600 payloads × ~0.41 µs ≈ 1.5 ms/frame outdoors (synthetic
lower bound); the zero-loss in-game VirtualQuery-inclusive exposure was 1.83 ms.
This is an estimate, not a current-version measurement. Remaining delta (native
traversal, other syscalls, driver) needs per-syscall attribution (ETW
SysCallEnter/Exit with stacks) to assign further WOW64 transitions.
