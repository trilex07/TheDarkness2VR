# Protected PassProbe test: incomplete FPS recovery

Exact isolated candidate `2e70cc4` (parent `c86c2c8`), diagnostic DLL SHA-256
`bdc7fd06b02c9a05242ab40a4e90999bb7b760482d940c6225d4e89a3b7e2b5f`.
This was one authorized Steam/PSVR2 test, not the integrated reader-fence
candidate. References remain legacy mode, source quality and last-consumer
fence are OFF; actual shared source is 1920x1071. Head, controls, gestures and
persistent logging are retained. No EXE-image writes.

User feedback: possibly slightly faster, briefly 60-70 FPS, then falls back
to about40. **The outdoor30-50 FPS bottleneck remains UNRESOLVED.** The diagnostic
probe is absent (zero PassProbe records), but its removed COM queries are not
established as the dominant frame cost. No 90Hz or performance acceptance.

The measured focused gameplay window lasted35.0209602s. Seven fully bracketed
FrameContext receipts have layers1 and matching game/eye-pair IDs. Six complete
five-second API-wall windows show boundary means2.467-2.953ms, original Present
0.337-0.347ms, xrWaitFrame0.022-0.040ms, reader fence0.216-0.250ms/read,
producer fence0.380-0.535ms/read. Respective maxima include boundary14.292ms and
producer11.841ms. Scopes overlap; no sum is presented. These API spans cannot
partition all reported20-30ms into CPU execution, blocked or Ready time. D3D9
GPU timestamp spans are elapsed spans, not GPUBusy.

Steam reports exit0. Last sampled native Present totals104000 entries,
forwarded, returned and successful all equal; recursion/errors/statuses/fault0.
No C0000005 is recorded. A bounded surviving test is not a long-term stability
claim. Fresh EXE identity/site audit passed; original DLL and all four INIs were
restored with5/5 matching SHA-256. EXE disk hash remains
`d46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1`.
Private raw logs remain outside Git.

Existing093553/log-A/B ETLs used the former EXE-patching architecture and
2048x2091 source. Their markers do not identify a level or outdoor transition.
Their logging/shader findings are historical, not attribution of today's
protected outdoor slowdown. Next required evidence is ONE complete system-wide
CPU sampled/precise + GPU ETW + PresentMon recording of indoor, transition,
outdoor stationary and combat, preserving exact DLL/PDB and explicit QPC phases.
No additional Steam start is authorized by this receipt.
