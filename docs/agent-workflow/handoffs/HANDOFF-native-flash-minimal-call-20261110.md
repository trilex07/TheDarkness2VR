# ONE immediate native Flash call: review-only, no image TTL

S20 cleanup accepted by MAIN. MAIN now owns Wonderlands487a82f0 acquired
16:14:31Z, cleanup<=16:22:31Z. CPU-only here; no lease/game/input/GPU/REA.

## Exact minimal action

tools/native_flash_once.py is a single CLI invocation, not another resident,
framework or input pipeline. Default CLI inert. It uses unchanged existing
CurrentPhase and focus owner guard. Its only native child is the exact639c
helper. No acquisition, launch, focus activation, image capture, hover, key,
renderer change, external client observer or retry in this file.

Future NEW grant required. Immediately after the agent actually views BOTH eyes
showing native Main from the CURRENT owned session, execute exactly:

```powershell
python -B C:/Users/Trilex/Darkness2VR/.aim-indicator-20261110/tools/native_flash_once.py --live --session <NEW_SESSION_ROOT> --packet C:/Users/Trilex/Darkness2VR/.aim-indicator-20261110/captures/aim-indicator/runtime-preflight-v1/packet.json
```

<NEW_SESSION_ROOT> is the actual fresh lifecycle root, NOT its session subfolder
and NOT S19/S20. Packet9A962 stays byte-identical. No helper check is relaxed.
This extra call file is a separately reviewed sidecar, not silently added to the
old28pins. No more per-slot pasted qualification code or new image-age assertion.

The function reads only the already-existing session/owned-window.json and
bootstrap/focus-expected.json. It requires their PID/creation/path/HWND/token
to match. The existing originalA+240 focus deadline becomes admitted A+230 by
subtracting10s; this is the original grant boundary, not a screenshot TTL.
The unchanged owner guard checks exact CURRENT lease, fresh process, inventory,
creation/path/HWND ownership and deadline. Exact native foreground is checked.
CurrentPhase reads the exact PID/session runtime-marker log with its unchanged
append/size/marker protections; current flags must be1 just before invocation.
STOP-INPUT/REQUEST-CLOSE/NAVIGATION-FATAL refuse. Helper must hash639c. These
checks execute synchronously; no external observer/capture sits before the read.

Then subprocess invokes the existing helper ONCE with ALL arguments:
--exe CURRENT_EXE --pid CURRENT_PID --hwnd CURRENT_HWND
--created-ticks CURRENT_CREATED --session-id CURRENT_SESSION
--owner-token CURRENT_TOKEN --output NEW_SESSION_ROOT/native-once.json.
Values come from the current emitted receipts, never historical literals.
It returns the helper's0/2 code without retry. A refusal preserves its output.
Afterward it rechecks owner/deadline/foreground. Normal lifecycle cleanup is
unchanged. Optional external client/clip follows this result only if admitted,
never delays the one Flash call. Actual Main image review remains the agent's
required visual step; no fake image recognition or capture-ID rehydration.

## Existing639c output contract, exercised through its ACTUAL CLI

Private captures/aim-indicator/native-call-review-v1:
fixture-success.json, fixture-refusal.json and output-contract.json are explicitly
SYNTHETIC CPU fixtures, NOT process snapshots or actual Flash dimensions.
Actual helper main/parser/file-output ran unchanged, with only live() replaced
by the existing fake-memory collect fixture (zero native APIs).

Success: return0, status READ_ONLY_EQUAL_PAIR, loaded_methods37, loaded_slots19,
identity/current session, helper_sha256639c, read_calls/read_bytes, snapshot.
Fixture took160reads/28,204bytes. Snapshot includes actual contract names:
manager_viewport, movie_extents, offsets, movie_viewport, input_scales,
viewport_copy, raw_movie_xy, mouse_xy_bounds plus ownership chain addresses.
Do not expect a separate filename per field, synthetic_fixture field or atomic
image/frame receipt. native_frame/graphics_cache remain SKIPPED; lifetime_lease
and atomic_snapshot false; input_admitted false; menu_phase not inferred.

Refusal: return2, status REFUSED, error string, input_admitted=false. Do not
expect snapshot/helper_sha256 or invent values when absent. Existing output
path reuse raises FileExistsError BEFORE live(): single-use output is intact.
Never catch that to choose another filename and retry the read.

Helper still checks37 loaded methods/19 slots and exact current PID/creation/
path/native HWND+foreground/lease before/between/after TWO equal snapshots.
Existing512read/128KiB/2s budget unchanged. No target calls/hooks/writes,
ephemeral native frame or unproved graphics-cache read.

CPU evidence:8 direct-call tests PASS (default inert, exact one complete argv,
native refusal not retried, unknown/changedphase, foreground/owner loss, lifecycle
stop, bootstrap identity mismatch, no image-file/age dependency); unchanged17
native-helper tests PASS; actual CLI synthetic success/refusal/replay PASS.
The direct-call tests inject native dependencies and a callback: they do not
construct WindowsBackend/NativeAPI or inspect a live process.

## Product/rollback status

No mod source/build/installed profile change. Dot4C2A/source494e2b43 and current
81FD/mainINI761F/diag1172 plus user gesture/retry/elevation1 remain intact.
Native active Flash scale data still NOT RUN; no menu or gameplay alignment fix
claim. Existing fresh backup/close350/input360/cleanup480 policy remains as9A962;
native call must start before originalA+230. Any future refusal stops the slot,
normal owned close/fresh restore/verify/release; no automatic reacquisition.
