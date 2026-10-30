# Bounded NVIDIA compiler dependency capture

This diagnostic extends the existing collector. It is not a compiler-stall fix.
The retained 2026-10-30 capture proves a19.749ms render wait released by a NVIDIA
worker executing nvgpucomp32, and a separate17.950ms producer-fence wait. It does
not identify a shader/pipeline or prove that texture allocation is the expensive
work. See ../investigations/combat-20261030-critical-path.md.

## Operation and safety

Diagnostic build only: `[Diagnostics] CommandHistory=1` in the game diagnostic
INI, default0. Ordinary builds omit the feature. VR, OpenXR, normal trace mode,
DirectRecordCopy, buffer versions, image scaling and synchronization flags are
unchanged. StateBlockHook stays0 in the accepted profile.

A fixed32768-entry ring retains8.25MiB of value records. There are no per-command
allocations, log writes, locks, GPU fences, readbacks or renderer waits. Each
record has QPC begin/end, monotonic sequence/draw IDs, frame ID, TID, live/replay
indicator (-1 live,0/1 replay), device address and Reset epoch. Producer-flush
records use argument0 for capture eye/slot. Record operations include all four
actual draw submissions, CreateTexture (caller, description, resulting resource),
shader creation (resulting pointer), blits/updates/clears and the existing
producer-flush boundary. Native calls, arguments, ordering and HRESULT are kept.

Pipeline fields are observed VS/PS pointers, declaration/FVF, four RTs/depth,
texture stages0..7,16 selected render states and six sampler0 states. They are
not complete driver pipeline keys. Pointer IDs are not shader hashes and may be
recycled: use creation events and generation. No resources are retained.

When StateBlock Apply is already observed, successful setters update the mirror,
failed setters leave it unchanged, and building a block does not update it.
Apply invalidates it. With the accepted StateBlockHook=0, actual VS/PS are queried
before each recorded command. Other fields are valid only when freshly queried;
full snapshots are capped at four per frame. Honor known/renderKnown/samplerKnown
bitmasks and uncertain; do not promote last-observed values to effective state.

COM reads balance acquired references. Reset blocks diagnostics and invalidates
the mirror; other-thread mutations invalidate it and mark overlapping commands
uncertain. Query overhead is accumulated separately. Reads over0.25ms are explicit
`diagnosticStateRead` records; they must not be blamed on the original engine.
The CPU fixture measured about0.63ms/5000 commands using fake getters: this excludes
real driver getter costs and ETW, and is not proof of acceptable runtime overhead.

The first measured operation >=12ms freezes the ring, including preceding
commands and any open outer calls. No stall means the final bounded history is
exported instead. Capturing begins at the first owner draw after arming; earlier
initialization and overwritten history are not available. Unowned threads are
not attributed to the render thread. A new game process is required to rearm.

## One authorized session

Preserve exact diagnostic DLL/PDB/MAP before another link. Independently verify
PDB GUID/age against the DLL RSDS; a matching DLL filename is insufficient.
Keep all four INIs and a verified rollback DLL. Prepare elevation before launching
the game; never trigger UAC while the user wears the headset.

Use the existing Capture-DarknessCombat.ps1 with:
`-CommandHistory -SymbolSource <frozen-diagnostic-directory> -DurationSec 30 -BaselineSec 5 -ArmDelaySec 15 -AudioCues`. The switch requires and preserves exact symbols
also in stationary/combat mode. It never launches the game or changes settings.

The collector signals a per-process arm event after WPR/PresentMon start. The
render thread never waits on control events. The export worker waits for the
collector export signal, which is sent AFTER WPR stops, and then reads only a
release/acquire-published frozen ring. The final five beeps occur after history
export. Keep the game open until then. The collector copies command-history.bin
and records SHA-256 in its existing manifest; it rejects a stale same-PID export.

Decode using tools/perf/summarize_command_history.py, optionally --around-qpc and
--before. Join exact event intervals with render RUN/READY/WAIT and worker stacks.
A nearby shader binding alone is not proof of compilation of that shader. Require
the releasing worker/dependency and account for all preceding queued commands.
If private driver attribution is unavailable, report that limitation and preserve
the strongest proved dependency. No speculative prewarming or fence removal.

## Offline verification

Both x86 builds and actual-source history/forwarding/export fixtures pass. New
checks cover ring wrap, nesting, failed HRESULT, StateBlock construction and
Apply, unobserved inherited shader bindings, query budget, Reset, foreign mutation,
immutable freeze, deferred export and binary layout/truncation. Existing StateBlock,
VB/IB callback/deadlock and live-shader synchronization regressions pass, as do
Present readiness, PresentMon analysis and31 collector helper checks.

Three old Python transfer-order checks initially errored on parent HEAD and the
first candidate: they searched d3d9.cpp for helpers now in shared_capture.inl and
old single-slot reader names. Updated the checks to the actual slot-based helpers;
all seven now pass. The renderer lifetime code was unchanged. New integration
checks verify that producer GetData FLUSH call count/flags are kept.

Steam/PSVR2/compiler attribution and stall reduction: NOT RUN. No90FPS gain or
visual fix is claimed. Native EXE bytes are unchanged.
