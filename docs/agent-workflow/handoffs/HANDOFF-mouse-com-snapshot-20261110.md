# MAIN-only mouse COM snapshot: frozen read-only helper

Branch codex/native-flash-alignment-20261110. No product change, native write,
hook, target call, focus activation, game launch or SIDE runtime execution.
NativeImageGuard and all native vtables remain unchanged.

## Exact invocation and output

```powershell
$Alignment = 'C:/Users/Trilex/Darkness2VR/.native-flash-alignment-20261110'
# MAIN supplies CURRENT unused session, exact emitted bootstrap receipt and
# canonical owner name. Do not substitute historical PID/HWND/session/token.
python -B "$Alignment/tools/native-camera/mouse_com_receipt.py" --live --expected "$NewSession/bootstrap/focus-expected.json" --owner-name $CanonicalOwner --output "$NewSession/mouse-com-once.json"
```

The existing bootstrap producer emits focus-expected.json (NOT capture.json).
Required fields: pid, hwnd, created_ticks (integer FILETIME), exe, token,
session_id and deadline (absolute UNIX seconds). Keep original current deadline;
never reconstruct the identity from historical S21 or extend an expired receipt.
Immediately before invoking, MAIN actually views current BOTH-eye Main and
checks current NativeMenu flags1 using the existing session log marker reader.
This helper does not infer phase from memory or pretend to recognize pixels.

Default without --live is inert. Output is exclusive-create; one invocation,
no retries on refusal. Exit0 means READ_ONLY_EQUAL_PAIR, exit2 means REFUSED.
Raw partial snapshots survive read/PE/identity refusal where already collected.
Success contains identity, snapshot.native (the original S21 movie/scale fields),
device/table/raw11slots, get_state/get_data, acquired_byte, exact native mouse
HWND, mapped module path/base/RVA, raw DOS/NT/section header hex and101 code bytes
(5 bytes before target plus96 bytes at target). No method signature is invented.
Two equal snapshots do NOT establish COM lifetime, atomic frame, acquisition
ownership, safety to hook or safety to call. All those admissions remain false.

Read budget unchanged from639c:512 reads,128KiB,2seconds. Rights0x1410 ONLY
(QUERY_INFORMATION|QUERY_LIMITED_INFORMATION|VM_READ). Exact creation/path/window
owner/title/foreground/alive process and canonical owner/token/current expected
file are checked before/between/after. VirtualQueryEx/GetMappedFileNameW require
one committed executable MEM_IMAGE mapping and bounded PE32 executable section.
No follow-jump, prototype, DLL loading into game, input, native call or write.

## Layout pins and imports

EXE SHA D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1.
Unchanged639c inspector verifies37 loaded methods/19 slots. This helper adds:
603190/1B,43E160/17,9A5D90/17A (including RET04),821400/79 and full mouse
RTTI/table FC00C4/124 bytes. Exact digests are in METHODS/source and review JSON.

Chain:10B389C -> manager ref/object FDA948 -> +58/+5C top movie ref ->
movie FDA6E8 (visible+79,cursor+F4,reciprocal ref+C) -> driver+44;
driver+48 inputref equals movie+7C -> input FBFAD8/FBFCD0 -> input+A44
mouse ref -> mouse FC00C8. Mouse+1E8 must equal exact HWND; mouse+7 acquisition
byte; mouse+1A4 device -> device DWORD vtable -> +24 GetDeviceState,
**+28 GetDeviceData**. All addresses are32-bit; function address need not align.
Actual external module/target bytes are UNKNOWN until MAIN's live receipt.

Frozen review: captures/native-flash-alignment/mouse-com-review-v1.json pins
helper, tests, this handoff,639c and its four existing local imports, original
CPU fixture, and exact S21 runner/packet/backup/bootstrap/focus paths. No game
bytes/captures are committed. CPU13 tests PASS after correcting one test's
expected error message to the actual unchanged639c rejection; no guard changed.

## Exact existing S21 runner and MAIN reuse boundary

Runner: C:/Users/Trilex/Darkness2VR/.aim-indicator-20261110/tools/aim_indicator_session.py
Packet: C:/Users/Trilex/Darkness2VR/.aim-indicator-20261110/captures/aim-indicator/runtime-preflight-v1/packet.json
SHA9A962FF06603E335D7140528AA4A91204A02CB266046782D5C667C7508757E1A.
Actual terminal S21 root (evidence ONLY; never reuse for backup/runtime):
C:/Users/Trilex/Darkness2VR/.aim-indicator-20261110/captures/aim-indicator/native-flash-s21-20261110-162151
Sidecar: same tree/tools/native_flash_once.py; it calls639c, NOT this new helper.
Original complete command sequence:
docs/agent-workflow/handoffs/HANDOFF-aim-native-preflight-20261110.md.

IMPORTANT: S21 packet deploys Dot4C2A, not installed81FD. Outer Adapter.lease
requires owner darkness2. Do NOT run it unchanged under MAIN owner or pretend
it leaves81FD installed. No current81FD MAIN-owner runner was frozen here.
MAIN should reuse its own installed81FD launch/normal-close ownership lifecycle,
with the existing fresh backup API below. No frozen packet/global source edits.

Minimal menu choosepoint: launch/startup title -> actually viewed Title/attract
flags2 ordinary released Enter (existing bounded title policy) -> observe actual
Main BOTH eyes/currentflags1 -> STOP ALL CONFIRMS -> invoke new helper once.
No Continue, Main A/Enter, arrows, hover, gameplay, cursor mutation or Dot cycle.
After receipt/refusal request early owned normal close and fresh restore.

## Fresh backup API (existing file, not historical data)

Module: C:/Users/Trilex/Darkness2VR/.unattended-audit-20261110/tools/darkness_session_backup.py

```python
# Within MAIN's existing no-games/current-token/deadline guard, before mutation:
plan = backup.inventory(game, local, roaming)
receipt = backup.snapshot(plan, NEW_UNUSED_FRESH_DIRECTORY)
```

Roots: Steam steamapps/common/Darkness II;
C:/Users/Trilex/AppData/Local/Darkness2VR;
C:/Users/Trilex/AppData/Roaming/DarknessII.
inventory caps64files/64MiB/2s; snapshot caps15s, records current absences and
all copied hashes, writes COMPLETE only after complete source+copy verification.
It does not itself establish runtime authorization or acquire/release a lease.
Protect loader/EXE/XR/Steam separately in MAIN's existing lifecycle. Restore only
THIS current COMPLETE manifest; preserve unknown files and user's option1 keys.

Full callback API also exists:
prepare_and_start(game,local,roaming,destination,lease_token=token,
stop_input_utc=input_deadline,cleanup_utc=cleanup_deadline,ownership=guard,
install=install_callback,launch=launch_callback,rollback=restore_callback).
BUT its validate_recipe uses historical39D/228625 unless an explicitly reviewed
private current-profile binding is supplied. The S21 binding has correct current
81FD/761F/1172 profile but candidate4C2A. Do not blindly call that full API for
this unchanged81FD MAIN trial. snapshot/inventory above have no profile override.

## Upload/shot same-frame evidence: existing fields and exact gap

src/native_view_witness.h ProjectionWitness records frame/uploadedFrame/
verifiedFrame, view/renderer/camera and verified owner pointers plus inverse,
expectedUpload/uploadedInverse/verifiedInverse. Existing NativeViewBoundary and
NativeProjectionPair logs can identify upload owner/frame when emitted.
Those globals are mod-owned symbols, not pinned native EXE addresses; this helper
does not read them or any ephemeral frame/cache.

src/protected_weapon.inl ResolveProtectedWeaponFire uses NativeGameplaySample
owner.camera (fresh time<150ms), equipped fire/model/gun ownership, camera+80
and actual production transforms. ProtectedWeaponShot logs frame/fire/ref/stage
but NOT owner.camera or final shot matrix. NativeGameplaySample has time, NOT a
frame identity. Thus installed81FD has NO complete existing same-frame
upload+shot-camera receipt. Equal frame log numbers alone cannot close this gap.
Future narrowly reviewed product receipt at actual resolver/shot-report boundary
would need camera/frame/sample-time/final-ray plus witness upload owner/frame;
not a new external observer or a guess that freshness proves same-frame.
