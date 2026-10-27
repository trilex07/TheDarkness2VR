# Native local constant capture candidate

## Evidence and scope

The ad8db6a game capture `material-window-capture-20261027T140923Z/trace/20261027T141205Z-c4e8bb85`
retains its ETL, exact DLL/PDB/MAP and raw PresentMon data under LocalAppData/Darkness2VR/runtime.
ETW and PresentMon both report zero lost events. The user manually closed the game during
combat; the whole run is INVALID/INCOMPLETE. Only the completed indoor/transition/outdoor
intervals support phase comparisons. User confirmed CPU ON, correct stereo, matched outdoor
view, and experiencing the hitch. The trace does not isolate that reported hitch reliably.

Completed outdoor: 559 PresentMon records, mean present interval 17.888 ms, GPUBusy 3.652 ms.
PresentMon CPUBusy is not scheduler RUN; GPUTime includes gaps. This is not HMD throughput.
Render TID 20940 has 8,823 sampled weights in the outdoor interval. RecPush-inclusive
weight 1,241 / 559 presents is approximately 2.22 ms/present at the nominal 1 ms sampling
interval, NOT an exact timer or removable saving. WOW64 NtReadVirtualMemory-inclusive
weight 1,731 overlaps it. These categories must not be added. Some native ancestry is absent
from 64-bit transition fragments; ntdll memset cannot all be assigned to the recorder.
536 sampled weights contain ETW-marked nonnative work; zero loss does not mean zero overhead.
The precise scheduler export is separate, expensive and not a basis of this change.

Timestamp-joined reducer: 0 stack timestamp mismatches. A full representative chain is:
native B74740 -> recorder RecPush -> KernelBase ReadProcessMemory -> ntdll ZwReadVirtualMemory
-> WOW64 whNtReadVirtualMemory -> kernel NtReadVirtualMemory -> MiCopyVirtualMemory.
Native ancestors include 71F71F/71F736, whose current-owner material builders are 571670/AD3380.
Other payload producers and partial stacks remain outside this optimization.

## Producer ownership and implementation

Read-only Ghidra project `%TEMP%/d2vr-rootcause-ghidra/RootCause.gpr`, program
`d2vr-rootcause-installed.exe`, verified SHA256
`D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1`.
Do not use the older Camera.gpr executable for this contract. Ghidra outputs are retained
in this capture's analysis-derived/native-constant-{abi,upload-contracts,producers}.txt.

VS B74720 returns to B74740; PS 96E3B0 returns to 96E3D0. These forwarders preserve EBP
and duplicate first-register/data/count arguments when calling the device method.
571670 and AD3380 align ESP from EBP, allocate 0x794 bytes, and save EBX/ESI/EDI. Admission
checks the exact active frame relation, duplicate arguments, bounded register range,
native caller ranges and known parent return addresses. Payload must lie entirely inside
this builder's local allocation, excluding its stack cookie. Current TIB stack bounds
are read on each call. Heap, foreign-frame, bone-array and unknown sources retain the old
ReadProcessMemory/fresh VirtualQuery fallback.

This removes a system transition for an already owned local buffer, not a redundant native
shader upload. It skips no draw/state/constant command and retains no source pointer across
calls. Existing dirty checks are not replaced. No source-page permissions are cached.
The direct copy relies on the native compiler-owned active-stack contract: normal automatic
guard growth is below the allocation, already passed by callback pushes. It is not a promise
that arbitrary manually protected stack pages can be treated as owned. An unexpected copy
exception disables admission permanently and requests the existing fallback.

`NativeOwnedConstantCopy=0` is the repository default. The diagnostic candidate can admit it
only after strict EXE SHA/base and loaded forwarder/prolog fingerprints. No EXE bytes or
native vtables are written. Without diagnostic image admission it remains ineffective.
L3+R3 package OFF disables this feature; ON restores it only if requested at startup and still
admitted. Existing green CPU ON/orange CPU OFF and triangle-hold recenter are retained.
Periodic NativeOwnedConstantCopy receipts are cumulative on the reporting thread and expose
attempts/proved/copies/bytes/faults. Admission alone is not proof that real-game payloads qualify.

## Offline verification and integration

- Production and diagnostic x86 DLL builds PASS.
- Native source test extracts the actual PS hook and exercises its frame-pointer capture
  through compiled x86 native-shaped builder/forwarder code: 1,000 successful copies,
  exact recorder/forward bits; 2,027 checks total. Includes argument/frame/range rejection,
  disabled/unadmitted behavior, an injected access fault with sticky disable, and preservation
  of a foreign PAGE_GUARD through fallback. The fault is deliberately injected, not a game fault.
- Actual RecPush baseline: 549 byte/range/guard/fallback checks PASS.
- Actual replay loop production/diagnostic: each 96 streams, 576,096 records, 70,090 checks PASS.
- XInput production 54 / diagnostic 108 checks PASS; final status PS WARP PASS.
- Transfer worker actual x86 D3D9Ex/D3D11: 400 transfers, pixel equality and reset/lifecycle PASS.

Integrate this commit after ad8db6a on feature/render-command-worker, build with
`build.ps1 -DiagnosticCapture`, preserve the installed profile and set only
`[Render] NativeOwnedConstantCopy=1`. Keep symbols with the DLL. Require startup admission,
nonzero copy receipts and zero runtime faults, then matched-scene OFF/ON measurement.
The scoped source-copy portion may be much smaller than the full 2.22 ms exposure.
Runtime benefit, hitch resolution and stable 90 Hz are NOT VERIFIED by these offline tests.
Do not replace MAIN's renderer, its settings, or its worktree when integrating.
