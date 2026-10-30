# Native input service: corrected initializer ABI

Static evidence only, D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1.
This resolves the specific initializer-argument uncertainty recorded in MODLOG;
it does not establish a live framework address or authorize native input writes.

9F4580 creates/adopts the input manager into its local reference at normal
ESP+14. The plain Windows path calls 63C950 at 9F48A5; other platform paths use
625670/8D89A0. Graphics and sound references occupy ESP+10/+18. At 9F4B01,
EDI is set to framework+A8; the client reference is assigned there at 9F4B1F.
At 9F4C55/57, ESI becomes the client object through that reference.

The misleading call at 9F4C6F has **no stack arguments and no stack cleanup**:
C32680 returns the service-global address 10DE3F8. Static initializer EA4FD0
calls C5CC40 for that object; its constructor sets vtable FBAC40. Slot +1C is
74B5F0, exactly `lea eax,[ecx+50h]; ret`. Three words pushed before this getter
are arguments for the subsequent C2C1A0 call, not getter arguments. No factory
or guessed calling convention is needed to resolve it.

C2C1A0 has client in ECX and seven stack arguments:

1. Framework object.
2. Address of service-global+50.
3. Address of local graphics reference (normal caller ESP+10).
4. Address of local sound reference (normal caller ESP+18).
5. Address of local input reference (normal caller ESP+14).
6. Framework+10 reference value, with its count incremented by the caller.
7. 1102080.

The initializer subtracts 2C and saves four registers. At its stable ESP,
reads +48/+4C/+50 name arguments 3/4/5. At C2C38C, C2C3A2 and C2C3B4,
7A8570 assigns the respective references to client+2C4 (sound), +2D0
(graphics), and +374 (input). Its RET 4 consumes the temporary argument each
time; C2C1A0's RET 1C consumes all seven original arguments. 7A8570 manages
intrusive reference counters: the stored field is not itself an InputMgrWin
object. No claim of exclusive ownership, thread safety or strong retention is
made from these offsets.

`python -B tools/native-camera/audit_input_service.py <installed EXE>` checks
the exact file SHA/PE identity and decisive constructor/getter/frame bytes.
It prints a static receipt and explicitly leaves live-object/type, menu state,
menu surface and injection admission false. The correct installed EXE passes;
a changed image is rejected. No game process, DLL, INI or Steam settings change.

## Existing window to framework ownership route

The framework is allocated by 560550 and retained in the main program loop
9C3E40; there is no proven fixed global framework pointer. The main-window
configuration method 40DEC0 writes D993C0 at configuration+44 (WndProc) and
the framework pointer at +48 (creation parameter). C27160 passes graphics+18C
to D14750, which copies both fields verbatim to graphics+1D0/+1D4. A0B540
uses these fields for window registration and CreateWindowExA.

On WM_CREATE, D993C0 obtains the creation parameter from CREATESTRUCTA,
stores it in GWLP_USERDATA (-21), and stores the same HWND at framework+F8.
Subsequent messages retrieve that userdata before calling the framework's
message handler. 40A300 is an auxiliary dialog, not this ownership route.
The framework constructor selects FC8930; the Windows input-manager
constructor AAE390 selects FBFCD0. The manager's mouse reference is stored
at +A44 by 8460E0/84BBA0; mouse constructor 821400 selects FC00C8. Reference
assignment is not a lookup and must not be called to discover an object.

Optional `--pid <existing-game-pid>` now reads the HWND userdata, validates
framework type and HWND mirror, follows both reference indirections, and
validates the input-manager type. It opens only query/VM_READ access and
compares loaded instruction ranges against the pinned file. Two matching
chain reads bracketed by window-identity checks yield a **read-only snapshot**,
not a lifetime lease or permission to write input. There is no process launch,
native call, hook, polling loop or memory mutation. No matching snapshot exits
with failure. The game was closed during this work: live observation NOT RUN.

Four ownership tests PASS, including invalid pointers, field overflow,
unreadable memory, wrong HWND/types and mistaken reference/object indirections.
The installed EXE static audit PASS; altered-image and invalid-PID rejection
PASS. No C++ source changed, so no new DLL/build/deployment was produced.

Next: observe the existing receiver during the consolidated authorized game
session, then establish a valid input lifetime and menu-only event contract.
The game's actual submitted menu surface and pause/menu state are separate
requirements; the existing compositor settings quad cannot certify them.

## Bounded in-process observation for the next manual session

The protected diagnostic build now observes the existing device HWND after
the independently SHA-pinned native player-owner admission succeeds. It reads
the chain above, then input manager+A44 -> mouse reference -> MouseDeviceWin
(FC00C8). Native updater 9A5D90 uses mouse+1A4 as its DirectInput receiver,
calling vtable+24 GetDeviceState and +28 GetDeviceData. These addresses are
recorded without calling either method. No factory or guessed object getter is
used. The observer writes no native memory and injects no events.

Two matching chains are bracketed by HWND PID/thread/userdata identity checks.
Reads refuse guard/no-access or uncommitted pages. At most eight attempts one
second apart are possible; the first successful observation disables further
reads. The existing private d3d9.log receives one NativeMenuReceiver line.
Production builds contain no observer operations. No INI switch or profile
change is required. If native owner admission is unavailable, this observation
is unavailable too; it cannot grant itself image identity.

The log explicitly retains lifetimeLease=0, menuState=UNKNOWN,
surface=UNKNOWN and injection=OFF. A matching snapshot does not justify future
dereferences after the observation or prove that mouse focus equals a menu.
Native callbacks require a separate owner-thread/lifetime contract; the next
game run is needed to confirm the actual receiver before selecting that hook.

Actual-source x86 snapshot fixture: 63 checks PASS, including missing fields,
wrong concrete types/HWND, reference-vs-object mistakes, address overflow,
alignment and changed snapshot rejection. The pinned installed EXE static
audit PASS. Live in-game receiver observation is still NOT RUN.

Bounded follow-up Ghidra queries also rejected two misleading entry routes:
C4D7D0 registers/looks up ActionScript member names including onMouseMove;
482D70/B8EA60 serialize input bindings including MENU_MOUSE_X/Y/Z. They are
not verified event dispatchers and must not be used to inject clicks.

## Region manager to game-rules reference

The pinned native registration table at 10A6150 associates `GetGameRules`
(string FB5E6C) with D9B8C0. This wrapper converts its receiver using the
RegionMgr SWIG type slot 10EBE00 and invokes vtable+13C with a local output
reference. RegionMgr's pinned FC7D00+13C contains 490D30. Its complete 18-byte
body loads the reference at receiver+BC, increments that reference's +4 count,
stores the reference in the caller's output, and returns that output in EAX
with RET 4. It does not return the GameRules object directly.

D9B8C0 copies the returned reference using 59AB10, releases the temporary,
and publishes the retained reference through the object's native type
metadata and SWIG. This closes the earlier uncertainty about what the
RegionMgr+BC getter represents: it is the native `GetGameRules` route.
The separate `GetGameRules` registration at 10BACB0 invokes DDD0D0 using
another receiver type slot, 1100A50, and helper 9E3600. These receivers must
not be interchanged.

This does not identify the live concrete game-rules vtable, prove that a
selected region exists during the main menu, or establish a rendered menu
surface. In particular, the already identified `IsPauseMenuShowing` +80
dispatch must not be called on the reference wrapper or an assumed object.
No production query or input injection was added. The remaining runtime
observation belongs in the consolidated manual session, before using this
route to distinguish native pause state from the controller's pause latch.

Read-only Ghidra exports all contain EVIDENCE COMPLETE; private export hashes:

- Native wrapper/getter query: ecb94fbb9184620d3aab09ae37fdc730463c9bb69eedcf2cf9f8c131f8e537c7.
- Reference assignment/publication: 5e2bdd3762c7367b1d00ec4d22bf3d62ba4d2c78bb51e32300f1e07e5e56a407.
- Verified getter/second wrapper: 59fd149b70168681eb8d4586aa9538dfddd80238141625b087b988b3b0d861bd.

Independent PE read verifies the vtable slot and all 18 getter bytes. No new
DLL, native patch, Steam deployment or performance result follows from this
static contract.

## Concrete native pause predicate, 2026-10-29

The previous uncertainty about the concrete implementation is now resolved
statically. Pinned registration EB2280 names GameRulesImpl, constructs its
descriptor at 10F3E80 and installs FAA470. Descriptor factory A6BDA0 allocates
450 bytes, sets the descriptor through 456960 and constructs through 4AC3A0.
That constructor installs instance vtable FD8240. Its +80 slot is A4D340,
matching the already recovered IsPauseMenuShowing wrapper dispatch.

The complete 29-byte predicate returns true exactly when the low byte at
receiver+198 is zero and the first word through the reference at receiver+EC
is nonzero. The nonzero +198 case short-circuits without reading +EC. The
meaning of that suppressing byte beyond this predicate is not inferred.
Several other static vtable slots reference the same method; unknown override
methods remain unsupported rather than being interpreted as this class.

ObservePause implements this read-only predicate after checking the exact
RegionMgr type/getter, following +BC through the intrusive reference, and
checking the actual rules object's +80 method. The existing owned view callback
passes its current manager directly: no saved manager address or singleton
scan is needed. Diagnostic builds make at most eight attempts, one second
apart, stopping on the first equal pair. The full loaded predicate bytes must
match. The NativePauseState receipt records its result, with lifetimeLease=0,
surface=UNKNOWN and injection=OFF. No native method is called and the result
does not yet route input. Production performs no observation.

This is an instantaneous snapshot, not a lifetime or coherent cross-field
transaction. In particular, equal reads do not rule out an ABA replacement.
It does not establish the main-menu state or a rendered menu plane, and does
not make the complete native menu laser ready. Live observation remains NOT RUN.

Actual-source x86 snapshot checks: 113 PASS, including short-circuit reads,
neighboring nonzero bytes, inherited method slots, missing references, unknown
overrides, overflow and unchanged output on refusal. Existing player-owner graph
4167 and actual view callback 143 checks PASS, including observer thread/image
gates, unchanged gameplay preparation and LastError preservation. Both full x86
builds PASS. The installed EXE SHA, full predicate and both getter slots match.
No FPS improvement or headset acceptance is claimed.

Private complete Ghidra exports are TEMP/d2vr-menu-rules-registration-20261029.txt,
rules-factory-body, rules-instance and pause-predicate with the same prefix/date.
Each ends in EVIDENCE COMPLETE; no extracted executable or decompilation is
committed.

## Private-framework receiver compatibility follow-up

The retained a508d5b5 game log reports eight failed receiver observations.
The observer required the stock FrameworkWin table FC8930, but mode3 publishes
our own SHA-pinned immutable clone of that same table before the observer runs.
This is a concrete source-level incompatibility; it does not establish that it
is the only runtime refusal. Observe now accepts the exact table supplied by
g_nativeViewTables[0].Table(), in addition to stock FC8930. It does not accept
the other view class, an observed arbitrary table, a neighboring clone address
or a weakened input/mouse/reference graph. The stable pair still is not a
lifetime lease and does not enable injection. The regression reproduces the
old rejection and tests clone admission plus retained graph/identity failures.
128 x86 snapshot checks PASS. Next consolidated game log must confirm whether
the real receiver is now found before input integration uses its implementation.

## Performance priority decision

The retained zero-loss outdoor capture 20261027T141205Z-c4e8bb85 has inclusive
weights 54/87 for C6AFEE/C6B029 over 559 presents: approximately .252 nominal
sampled ms per present combined. These are different calls to the two material
dispatchers, not exclusive removable work or a current-HEAD FPS measurement.
425C70/9DCC90 select per-material/per-object parameter overrides and defaults;
there is no proof that these submissions are redundant. This path does not
justify another large reconstruction as the substantial CPU optimization.
Do not add its exposure to overlapping RecPush/RPM or module totals.

The larger WOW64 memset bucket has no native caller in its largest retained
mixed-architecture stack; its producer is the service dispatcher, already
documented in outdoor-cpu-packets-20261026.md. The recording arena already
avoids resize value-initialization. Neither observation supports removing
native resource work, disabling required synchronization, or enabling unsafe
DirectRecordCopy. Current accepted Steam performance/rendering code is intact.


## Native cursor API recovered, 2026-10-30

Independent pinned-PE reads and complete Ghidra exports confirm MouseDeviceWin
FC00C8 slots +6C -> 603190 and +70 -> C4B690. The first is a void thiscall getter
with two output int pointers, reading +21C/+220; the second is a void thiscall
setter taking two client-pixel integers. C4B690 uses the mouse-owned HWND at
+1E8, ClientToScreen and, when acquired (+7), SetCursorPos. It updates the native
position after a successful warp, or directly while unacquired. Do not call it
from a saved read-only snapshot or assume that a render-target pixel is a client
pixel. No native call or input injection was performed here.

The existing direct OS cursor route alone is insufficient: 585450 reads
GetCursorPos only while unacquired. The acquired path's decoder 8A1470 (slot
+5C) consumes 20-byte events, accumulates offset 0/4 deltas into +21C/+220 and
calls 787DF0. Its bool return tests event appData == 1; it is not a button-down
flag. Offset 8 (wheel) is rewritten to event 9/10 and copied to the +230 pending
slot for a following update. Preserve this native event lifecycle rather than
manufacturing a guessed Flash MouseEvent or duplicating wheel transitions.

This narrows the intended integration to the existing native cursor/input
funnel. Actual submitted menu geometry, client/render coordinate mapping,
main-menu admission and owner-thread/lifetime still require proof before routing
controller events. The previous read-only snapshot does not establish them.
No full native menu laser or FPS improvement is claimed.

Private exports: TEMP/d2vr-menu-cursor-contract-20261030.txt,
d2vr-menu-mouse-events-20261030.txt and d2vr-menu-mouse-dispatch-20261030.txt;
each has the pinned EXE SHA and EVIDENCE COMPLETE. The existing input-service
audit now checks the concrete vtable entries and complete getter bytes.

## Exact receiver refusal receipt, 2026-10-30

The retained runtime still refused all eight observations after private-table
compatibility was added. That log does not identify which later graph check
failed. The existing bounded observer now reports the first failed field/type
check, read address, readability and expected/observed word on its final
attempt. It retains all admission checks, maximum eight attempts and no input
injection. No additional polling or profiler was introduced. Failure output
never publishes a partially resolved receiver. Both x86 builds PASS; 136
actual-source snapshot checks and 181368 pointer-core checks PASS. The installed
StateBlock/font candidate remains unchanged. Native menu routing remains
incomplete until the real receiver and submitted surface can be admitted.

## Observed base Windows input receiver, 2026-10-30

The consolidated retained game log from the 5ef2106c candidate identifies the
actual first refusal: input-type, readable FBFAD8 where the old observer
required only FBFCD0. It had already resolved the native framework, its HWND
and both client/input reference wrappers. No native pointers are retained.

Pinned-image disassembly and complete private Ghidra export confirm FBFAD8's
4CE810 constructor, its +A44 intrusive mouse reference, +60 -> 8460E0
initializer, allocation of a MouseDeviceWin through 821400 and reference
assignment through 84BBA0. FBFCD0's +60 -> 479EB0 invokes that same base
initializer. Both exact types therefore share the observed mouse path.
Observe now accepts just these two tables, retains the exact FC00C8 mouse
check and includes inputTable in snapshot equality/receipts. A type transition
within an otherwise identical graph cannot produce an equal pair. Unknown
neighboring tables remain rejected. This fixes the demonstrated rejection;
it does not turn a snapshot into a lifetime lease or enable native events.

The new regression fails against the previous actual source at its observed
base-type case, then passes after the correction. C++ snapshot 149 checks PASS;
five Python ownership tests PASS. Private-image auditor verifies both
initializer slots and the complete constructor SHA rather than publishing
new extracted executable bytes. Actual cursor/input owner-thread integration,
native menu surface and headset behavior remain unverified. No Steam files
are changed; the accepted 5ef2106c recovery baseline remains authoritative.

Private export: TEMP/d2vr-menu-observed-input-20261030.txt (pinned SHA and
EVIDENCE COMPLETE), not committed.

Both standard and protected-native x86 DLL builds PASS after this change.

## Concrete input integration boundary, 2026-10-30

Complete pinned Ghidra exports of 9A5D90, D993C0 and 744830 establish a
constraint that changes the implementation choice. The native mouse updater
keeps its current receiver in ESI and calls GetDeviceState first (return
9A5E3C), then GetDeviceData (return 9A5E70), with 20-byte events and flags 0.
It copies current state to previous state, clears current state, and polls
the immediate state before buffered events. Mouse fields +1C0/+1C4/+1C8
are state extent/current/previous; +1A8/+1BC are event buffer/capacity;
+1B8/+1CC are returned event count/decoder index.

The updater overwrites each returned event's appData with zero. On buffer
overflow (HRESULT 1), differing immediate states dispatch virtual +68 with
argument 1, rebuilding transitions. A bridge that changes only buffered
clicks therefore fails to preserve the native state/recovery contract. A
bridge must merge both immediate and buffered button state, preserve HRESULT
and capacity, and release only its own synthetic button ownership. It must
not mark buffered events with appData and expect that mark to reach the decoder.

The WndProc routes ordinary messages through 744830. WM_MOUSEMOVE does not
set native cursor coordinates in the standard mouse-message branch; other
mouse messages primarily control acquisition. A separate client-native mode
can consume messages first through a different receiver. Posting generic
Win32 mouse messages is not a demonstrated replacement for the DirectInput
state/event funnel and is not selected for the bridge.

The existing private-image auditor now fingerprints all three concrete
functions and emits this exact polling contract. It performs no additional
live polling, calls or writes. Private exports remain outside Git:
TEMP/d2vr-menu-input-update-20261030.txt and
TEMP/d2vr-menu-window-input-20261030.txt; both have EVIDENCE COMPLETE.
Native mouse callback lifetime/thread admission and submitted menu surface
remain required before controller events are injected. Accepted Steam baseline
unchanged.

Primary input-structure/state reference:
https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee416611(v=vs.85)
https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee416236(v=vs.85)
