# Native menu product route: pinned Steam COM and borrowed mouse publication

Isolated branch codex/native-flash-alignment-20261110 after Dot4C2A/source494e2b43.
One behavior: native Main ray/cursor alignment. Input NativeFlashRayCursor remains
OFF by default. No installed81FD/761F/1172 file or user's option1 keys changed.
No SIDE lease, process inspection, game launch, GPU, input or deployment.

## Actual ownership evidence and unchanged guard

MAIN receipt (PRIVATE, not committed):
C:/Users/Trilex/Tiny Tina VR/captures/darkness-mouse-com-main-20261110/run/mouse-com-once.json
SHA8B873165F4AB2A35049B87DA8D38DC530C8112D120B4F1757D11691E77DE5F9E.
MAIN observed native Main in BOTH eyes/currentflags1. Actual PID23516,
creation134388093862517192, HWND146407946.41 loaded methods,188reads/30,970bytes,
two equal bounded snapshots. No atomic frame/lifetime/hook admission inferred.

Device256481688 -> table1766852988 -> COM+28 target694723D0. Owner is
GameOverlayRenderer.dll base693F0000, RVA823D0, timestamp6AD92A09,
SizeOfImage18C000. This is Steam's DirectInput wrapper, NOT direct dinput8.
It either suppresses buffered data during overlay capture or forwards exactly
five stdcall arguments to the underlying COM object at wrapper-this+4, slot28.
Both observed exits are RET14. No guessed prototype or downstream call is added.

The recorded96 bytes SHA CAA3DCB06BFE249183454E14D7740C570490A870FBD97755ED1A3AD49782E047
contain TWO absolute PE32 operands at offsets5 and21. Only these are normalized
from actual module base to preferred10000000, after requiring exact RVAs147D5C
and147D5D. Normalized complete96-byte SHA:
D6EB3D69833B60C950B03096C7CEBC040E8051AEADDD33F015AE37BD29F4DBC6.
The local on-disk Steam module's same method matched that normalized body in
the offline trace. Its actual PE relocation directory independently contains
HIGHLOW entries823D5 and823E5. Private external-com-static-trace.json records
this static proof and file identity; no whole-file hash gate/arbitrary masking.

ExternalMouse::Start validates live module allocation/committed executable
MEM_IMAGE for target-5..target+95, exact module handle/name/RVA/PE32 timestamp/
size and96-byte method pin before MinHook. Target and replacement modules pinned
before publication; trampoline published before enable. Existing MinHook only.

Actual UNCHANGED Guard::Allows admits recorded external span694723CB/101.
It continues rejecting603190,43E160,CB7259 and FDA6E8 static table writes.
No guard/integrity check is weakened. New native image hooks: ZERO.

## Actual production path

1. The external wrapper gate captures native return address and ESI before C++
   can reuse them; public stdcall stack/RET20 is unchanged.
2. Only caller9A5E70 on exact owned native window thread can admit ESI mouse.
   Full9A5D90/17A and constructor821400/79, getter603190/1B, inputgetter43E160/17,
   full30-entry mouse table+RTTI and original Flash transform/updater/viewport/
   dispatch contracts are byte-pinned before installation.
3. AdmitMousePoll checks actual this/device/buffer/capacity/20-byte format/count/
   flags/window/current+1F4/previous+208/index0. Native GetDeviceData forwards
   EXACTLY once for every caller. Successful0/1 plus identical admitted owner
   fields after return are required for publication. Original HRESULT/LastError
   and all immediate/buffered data/counts/buttons remain native.
4. Under this stack lease, InstanceTable::Publish CAS changes ONLY this live
   MEM_PRIVATE writable mouse object's vptr. Source FC00C8 and every native
   static vtable/EXE byte stay unchanged. A process-lifetime READONLY clone
   preserves RTTI and all30 original entries except getter slot6C/index27.
   No instance address or lease is retained. Future calls are owned by the
   native caller's live object; destructor remains original. Known private
   clone equality is explicit, not acceptance of arbitrary foreign tables.
5. Native43E160 tail-jumps that getter, retaining callerCB7259. Original603190
   is called once first. Override only when outputs are exactly current top
   movie+E0/E4 and actual reciprocal movie/driver/input/mouse owner matches the
   getter receiver. Main/focus/native window thread/acquired mouse/current
   submitted Flash texture/crop/tracked hand/150ms surface checks stay intact.
6. Getter publishes RAW render-viewport point. Original6D56B0 transforms it
   afterwards exactly once; original591650 publishes MOVE/buttons unchanged.
   No physical HWND/client/ClipCursor scaling or eye-cache substitution.
   Existing confirm readiness records the corresponding final Flash point.

This is not the forbidden earlier static FDA6E8+148 installer: that entire
installer publication/write path has been removed. NativeFlashCursorHook remains
an uninstalled CPU reference callback; no production slot points at it.

## Meaningful CPU proof and build

Commands (absolute private OutputDir):

```powershell
./test/build_native_flash_cursor.ps1 -OutputDir C:/Users/Trilex/Darkness2VR/.native-flash-alignment-20261110/captures/native-flash-alignment/external-com-v1
./test/build_native_menu_mouse_poll.ps1
./build.ps1 -DiagnosticCapture
```

Actual first command: legacy RED exit1 Flash255/183 vs required666/509;
GREEN51218 checks including51005 complete native SSE transform comparisons.
Actual603190 fixture executes through the cloned getter SLOT and actual
production getter, actual
private InstanceTable publication, rawE0/E4=(1990,2970), then unchanged native
6D56B0 produces666/509. Native mouse stored761/1071 unchanged. Actual591650
event/button controls retained. Wrong caller, stack outputs, replacement mouse,
Title, OFF and shared context/ownership/tracking/surface negatives refuse.
Complete actual external96-byte pin passes, each96 individual byte mutations
and wrong relocation module refuse; exact recorded external guard span passes.

Second command:18092 checks PASS. Actual x86 gateway verifies ESI/caller, stack,
nonvolatile registers, exact arguments, HRESULT/LastError and one original call.
Actual production ForwardBorrowedMouse transaction accepts0/1, rejects failed
HRESULT, wrong thread/caller, changed device/HWND, oversized returned count and
foreign clone, always forwarding once. No API/object callback is cached.

Full x86 O2 DiagnosticCapture product build PASS. Initial fixture signature and
full-product namespace include integration errors were corrected; terminal logs
external-com-build-final.log, external-com-poll-v1.log,
external-com-snapshot-v1.log and external-com-product-build-final.log carry
actual results. Exact D46 denial controls retain the pinned EXE range. Private
allocation controls use the fixture's real ASLR PE image extent with the SAME
Guard implementation (synthetic fixed image ranges can overlap CPU allocations).
Existing native-menu snapshot regression additionally264 checks PASS; default
unknown-table refusal is preserved with the new explicit known-clone argument.

Artifact/source freeze: captures/native-flash-alignment/external-com-frozen-v1.
Review manifest records full source inputs, product artifacts, actual receipt
hash and terminal CPU evidence. Not a final combined Steam candidate/freeze.

## Exact remaining gates

Runtime NOT RUN; Steam wrapper suppression/lifetime interaction and actual
hover/highlight at center/edge/resized Main still require MAIN review/evidence.
Provider change, absent Steam wrapper, changed method or any owner mismatch
refuses this route. No fallback EXE/static table patch or guard relaxation.
The unchanged639c inspector requires native FC00C8; after this opt-in instance
publication it is expected to refuse that changed vptr. Do not relax/repin it.
Existing product logs separately mark first borrowed publication and bounded
CB7259 raw/final points; external route-ready alone proves neither action.
No live recipe requested by SIDE. Rollback is MAIN's fresh CURRENT81FD/INI/
saves/settings/loader/XR/Steam receipt, never historical39D or older saves.

Horizon packet candidate6b13/2D8397F2 is separate and unchanged; user-reported
cause is not accepted without physical/XR source/typed LOOKY evidence. Actual
shot/upload camera owner same-frame receipt remains missing on installed81FD;
same-owner shot/render math has passed offline, not real impact acceptance.
No claim this menu change fixes either remaining scope.
