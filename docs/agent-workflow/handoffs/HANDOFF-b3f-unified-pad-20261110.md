# B3F menu + UnifiedPadPackets isolated CPU integration

## Ownership and scope

Branch: `codex/b3f-unified-pad-integration-20261110`.
Worktree: `C:/Users/Trilex/Darkness2VR/.b3f-unified-pad-20261110`.
Base exactly `fab0a09d3bd73eeb6f37211d3bd49f480de630d3` (frozen B3F).
Donor exactly `6b13e0427335552cf0d6fb60ec6acc263014614c` (isolated 2D8397).

Only donor production changes were applied:15 lines in d3d9.cpp, the28-line
pad_packet_sequence.h and three INI lines. Existing three donor CPU fixture
files were imported exactly at Git-blob level. No donor runtime scripts,
reports, profile, adapters or historical packet bindings were transplanted.
Patch applied cleanly; no manual semantic conflict resolution. New notes are
local to this integration. All other B3F production files stay unchanged.

`Input NativeFlashRayCursor=0` and `Input UnifiedPadPackets=0` remain defaults.
The former retains pinned external COM borrowed lease/private instance getter;
the latter unifies returned port0 packet numbers when VR gamepad is enabled,
after physical/XR selection/remapping. It never changes Gamepad payload bytes.
No EXE/static vtable writes, NativeImageGuard relaxation, controller/button,
physical mouse, ClipCursor, fire transform, reload, throw or Cinema changes.
This does NOT include separate d530 ValidatedWeaponPreview.

## Actual verification

All commands below ran in THIS worktree, with absolute private output paths:

```powershell
./test/build_horizon_input.ps1 -OutputDir C:/Users/Trilex/Darkness2VR/.b3f-unified-pad-20261110/captures/b3f-unified-pad/packet-test
./test/build_native_flash_cursor.ps1 -OutputDir C:/Users/Trilex/Darkness2VR/.b3f-unified-pad-20261110/captures/b3f-unified-pad/flash-test
# TEMP was set only in the child shell to this worktree's captures/.../temp:
./test/build_native_menu_mouse_poll.ps1
./test/build_native_menu_input_snapshot.ps1
./test/build_vr_settings.ps1 -OutputDir C:/Users/Trilex/Darkness2VR/.b3f-unified-pad-20261110/captures/b3f-unified-pad/settings
./build.ps1 -DiagnosticCapture
```

- Native packet:2423 checks PASS. Actual89BB0F RED drops neutral on equal
  physical/XR packet ID; GREEN feeds neutral into actual typed LOOK_Y. Actual
  selector/finalizer extraction, native body pins, payload preservation,
  OFF/disabledVR/nonzeroport, reconnect/stability/wrap controls retained.
- Native Flash: legacy RED exits1 (255/183 vs666/509), GREEN51218 checks,
  including51005 native transform comparisons. B3F getter/publication/ownership
  and event/button controls unchanged.
- Native mouse polling18092 PASS; native menu snapshot264 PASS.
- All7 existing settings fixtures PASS:35/5789/2015/50/7215/68/20207 checks.
- Full x86 O2 protected-native Release (`DiagnosticCapture`) PASS.

The first full build refused missing assets/weapon-wheel/icons/weapon_icons.h.
It was restored from EXACT fab0a09d Git blob, SHA256
CDA3F8785821E0941EC54D00B7E227351DB89CB0D1B31B5CE96211F74CB776F6.
Third-party materialization used only13 missing records byte-verified against
the original B3F frozen manifest. No arbitrary dirty root inputs were copied.
Final successful build log is product-build-final.log; failed initial log is
preserved. No icon regeneration or source workaround was introduced.

## Frozen review package

`captures/b3f-unified-pad/frozen-v1/manifest.json` records exact source commit,
input/build/test/artifact hashes and scope. `manifest.sha256` is its digest.
Files are ignored/private; no native binaries or generated native body fixtures
are committed. Exact source commit is in that manifest and Git history.

- DLL SHA256 E6B8B6058CBE07126753D0C799DD031CA81E152FAD3503D856B1AFA22A85A97B
- PDB SHA256 0181F5A6D5068DB1D07E5B10A7F057849291A3B81DF029EC25CF26E5D1F258FB
- MAP SHA256 193EF70C5BE0D32C9A705F91A21EA736E29D7C30CA2E149F821D42CB663A3304

Original B3F260, d530272 and isolated horizon591 frozen records all rehashed
with ZERO mismatches (1123 total). Original recipes/manifests/worktrees were
not modified. Local original-freezes-checked.json preserves that comparison.

## Remaining runtime/world/horizon gates: MAIN only

This removes the product composition blocker: one binary now contains B3F's
menu getter and the horizon packet finalizer. It is a CPU review candidate,
NOT runtime-tested, installed, Steam-ready or an accepted horizon fix.
The user-session physical/XR handoff and causal horizon failure remain OPEN.

MAIN's bcb061 actual B3F one-hover OPCJE/red-ray BOTH PASS supports the MENU
component only. Continue/click/world were NOT RUN even in that prior binary.
Use the existing same-object UiAutomation ordinary primary/A8-sync then
finally-release actuator, fresh current BOTH image with actual visible Continue
selection, owner/session/foreground/phase/deadline guards. No C-only
live_main_a wrapper: its pointer-disable requirement is absent here. No blind
Enter, stale B3F pixel reuse or assumption that nativeflags imply actual world.
No new navigation framework or runtime binding was made by this task.

Any later MAIN-reviewed trial needs a NEW packet bound to this DLL and fresh
CURRENT rollback; never swap this DLL inside a frozen B3F packet. Candidate
profile may explicitly request NativeFlashRayCursor1 + UnifiedPadPackets1;
CameraShakeTrace1 only for the already documented bounded camera trial. Preserve
current user DoubleShakeReload1/DeferredGripRetry1/ThrowSolverFailElevation1,
all buttons and every unrelated INI byte. Never install the repo INI.

Existing horizon trial details remain in the UNCHANGED original
`.horizon-packet-20261110/docs/agent-workflow/handoffs/HANDOFF-unified-pad-camera-trial-20261110.md`:
actual world BOTH first, fixedpose6s/oneY0.25 for250ms/finally-zero/neutral8s,
pitch-rate comparison,30s trace window, input360/close350/cleanup480. No RB,
recenter, shots or bar. XR-only cannot force a physical/XR source collision.

No game/GPU/input/XR/installation/lease or shared installer edit was performed.
Current81FD/761F/1172 and explicit user options were not changed. Future rollback
must restore MAIN's NEW current backup/absences/protected state, never a prior
session or historical saves. Actual shot alignment and user horizon cause are
still unproved; no final freeze/Steam acceptance claim.
