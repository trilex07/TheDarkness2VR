# Native resource-cycle offline audit

This tool consumes existing Ghidra exports and already extracted sample JSON.
It never runs the game, attaches to a process, collects ETW, changes configuration
or writes the executable. PASS means static evidence was admitted, not that a
native optimization is safe or faster.

Requirements: Python with `pefile` and `capstone`; the private pinned EXE; existing
Ghidra RootCause project. Use only SHA256 d46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1.

Use the existing `tools/native-camera/CameraEvidence.java` with Ghidra
`-process d2vr-rootcause-installed.exe -readOnly -noanalysis`. Export to a private
directory OUTSIDE Git, with these exact file names and explicit addresses:

- entry-contracts.txt: 00ac36f0 00bb79a0 00bf27a0 009f2030 00c5d850 006ee720 0071f690
- parent-contracts.txt: 006f0190 005969e0 008f8150 007a43f0
- scheduler-config.txt: 00ff8d48 00f5ae28 00fb2804 00fac818 00f659a8 00f74b5c 00681e60 00609030 00714b50 00750cf0

No entry-byte override, new function creation or project analysis is needed.
Each export must identify the pinned image and finish with EVIDENCE COMPLETE.
The auditor trusts analyzed function ownership in those exports, not proximity
of a PC to a guessed function start. It validates selected contracts independently
against actual PE instructions. This is not a complete native ABI proof.

Example from PowerShell, after assigning your verified private paths:

```powershell
python tools/native-engine/audit_resource_cycle.py `
  --exe $pinnedExe --exports $privateExports --samples $existingSamples `
  --thread 28400 --phase combat --presents 1141 --output $privateOutput
python -m unittest discover -s tools/native-engine -p test_audit_resource_cycle.py
```

The1141 denominator belongs ONLY to raw B combat phase rows in
cpu-candidate-log-ab-20261026T183121Z/B/20261026T184245Z-9997b65a,
analysis-derived/cpu-phase-samples.json. For the older protected-outdoor-retry
session's root cpu-phase-samples.json, render TID42504: indoor708/outdoor490.
Do not reuse these identifiers for another trace. `partial=false, exit=0` checks
extraction completion; ETW loss and capture acceptance remain separate evidence.
Never add inclusive costs to these disjoint leaf counts. Zero samples do not
prove a function never executes. Unknown descendant/caller costs remain unknown.

Output is written only after input validation. It contains derived facts and
hashes, no decompiled game code. Parent output directory must already exist.
Tests exercise auditor admission/counting; they are not engine behavior tests.
