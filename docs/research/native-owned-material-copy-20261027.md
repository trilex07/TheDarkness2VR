# Fixed-field material constant capture

Follow-up to a57e131; keeps all native rendering and recording commands.

## Current runtime evidence

The user's a57e131 run log (PID 31044, render TID 9836) confirms strict image
admission and cumulative 1,491,991 copies / 9,277,452 inspected uploads, zero
copy faults. The package was toggled during this run and ended OFF. This confirms
the previous local-buffer path works, but establishes neither matched improvement
nor 90 Hz. It only covers approximately 16% of inspected calls in this mixed run.

The retained zero-loss ad8db6a capture exposed nominal sampled RecPush-inclusive
CPU around 2.22 ms/present in its completed outdoor interval. Its incomplete
combat interval does not establish the specific reported hitch. This new change
targets another portion of that same measured system-read cost; it does not prove
all outdoor CPU cost is here. See the previous native-owned-constant-copy document.

## New execution contract

Read-only Ghidra on the pinned D46BB6... EXE: both 571670 and AD3380 assign
EBX=ECX at entry, never write EBX again before the epilogue POP. Native forwarding
wrappers B74720/96E3B0 preserve EBX and EBP. Owner + 0x1C supplies the current
parameter object; builders read its members through +0x5E0 in this same call.
Nine immediate field submissions have these exact offset/vector-count pairs:
0x10/12, 0xD0/4, 0xD0/1, 0x110/17, 0x220/4, 0x4F0/6, 0x470/4, 0x260/4, 0x580/1.
Each is restricted to its exact stage-specific return address after the native
indirect call, not merely to a broad function range. Source must equal the exact
field start and length, on the active native builder frame and known parent chain.
Dynamic arrays, nested owners, bone buffers and arbitrary heap pointers are excluded.

Ownership relies on this live native object contract, not on treating arbitrary
heap memory as safe. It does not authorize manually protected/freed/corrupt native
objects; an unexpected direct-copy fault permanently disables admission and falls
back. No source permissions or native pointers are cached between calls. Unknown
contexts do not walk owner pointers. All ordinary guard/fallback tests still pass.

The naked x86 D3D9 entry gate passes device/start/data/count and original
slot/EBP/EBX by value to the ordinary C++ hook. Its stdcall implementation removes
28 argument bytes; the public gate removes the original 16 bytes. Return HRESULT
and callee-saved registers retain the D3D9 ABI. Camera witnesses receive the real
native caller address, not the internal gate return address.

Admission retains strict disk SHA/base and native wrapper fingerprints and now
checks all 8,416 bytes of each loaded producer body (FNV 4E52AC82 / 180DE9D4).
This detects changes to the EBX/field contract beyond the entry prologue.
It writes neither EXE bytes nor native tables. No D3D operation is deferred.

## Validation and use

The actual PS hook and common production gate are extracted into the x86 fixture:
4,051 checks PASS, including 1,000 local and 1,000 owner-field sequences, all nine
field extents with nonzero bit equality, unknown fields/offsets/owners rejected,
option OFF, owner-block replacement, invalid argument/frame bounds, foreign guard fallback, and deliberately
injected access fault with a sticky latch. The fixture's site policy contains its
own code addresses; no runtime policy override is shipped.

An offline 20,000-call 64B capture + stub-forward timing was RPM 617.1 ns/call,
owned-field 35.0 ns/call. This is a CPU fixture result, NOT a driver benchmark,
game-frame saving, hitch result or HMD acceptance. Real coverage is still unknown.
Actual RecPush guard/range/fallback549 and actual replay-loop production/diagnostic
2x70,090 checks PASS. Production and diagnostic DLL builds are required before deployment.

Build diagnostic DLL on feature/render-command-worker after a57e131. Preserve the
installed profile; set NativeOwnedConstantCopy=1 and NativeOwnedMaterialCopy=1.
Repository defaults stay OFF. L3+R3 CPU package OFF disables both paths; ON restores
only requested features. NativeOwnedMaterialCopy startup and periodic materialCopies
receipts establish admission/coverage. Require nonzero game materialCopies and zero
faults, then matched crowded-scene measurement. 90 Hz and hitch repair remain NOT VERIFIED.
