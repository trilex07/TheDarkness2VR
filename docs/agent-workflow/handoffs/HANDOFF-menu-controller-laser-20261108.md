# Handoff: controller laser in native menus

Date 2026-11-08. Task menu-controller-laser-20261108. Role MAIN (user assigned).
Branch codex/menu-controller-laser-20261108.
Worktree C:/Users/Trilex/Darkness2VR/.side-mod-audit-20261107.
Base6983f143. Installed prior candidate F11472BE (heart pull); accepted BASELINE
is distinct. Read relevant menu/input handoffs, FAILED-APPROACHES and workflow.

## User scope and implementation

Allow the right-controller laser to point/select in the ordinary game menu,
in addition to VR Settings. User does not use eye tracking. Keep pad navigation.

- src/controller_menu_pointer.h: projection on the actual menu surface and
  fresh native-cursor convergence check before trigger confirmation.
- src/eye_gaze.inl: reuse existing verified mouse/Flash cursor adapter with the
  right aim ray taking precedence over optional gaze. Front menu uses submitted
  cinema quad. Pause/options/talents use the NativeCameraUiScale plane already
  used by the settings entry. Cursor surface requires successful xrEndFrame,
  same XR space and age<=150ms. Game foreground/focused/tracked hand required.
- Existing cursor contracts preserved: FC00C8+70->C4B690 setter bytes,
  B072E0 movie cursor flag setter, existing mouse receiver graph and clamp fields.
  No guessed native offsets, new executable patches or mouse-click injection.
  Existing owner-thread closed-loop relative mouse MOVE adapter is reused.
- src/controller_menu_laser.inl: dot and beam from the same surface/ray. Reuses
  released solid texel from independent weapon-laser renderer, honors layer cap.
  Menu laser is automatic and independent of optional gameplay weapon laser.
- VR Settings panel/entry hover takes precedence. No laser in ordinary gameplay,
  QTE/story choice context, absent tracking, absent surface or other application.
- Existing R2->A route waits until native cursor reaches the ray within8 pixels.
  Once accepted, one continuous A hold persists until trigger release, so moving
  off/back while held cannot cause repeated confirmation edges. Physical A and
  ordinary pad fallback remain. No native mouse slider drag support added.

## Verification

- test/build_controller_menu_pointer.ps1 PASS339 checks: front/pause extents,
  miss/backface/space/frame rejection, stale/out-of-position cursor, actual
  production ApplyMenuTriggerConfirm body including delayed and latched confirm,
  gameplay trigger pass-through and physical A/B preservation.
- test/build_vr_menu_pointer.ps1 PASS181368 geometry/capture/navigation checks.
- test/build_vr_settings.ps1 PASS5787 input/laser,1754 renderer,50 actual host,
  7215 actual submission,29 preferences,9248 weapon-laser gates.
- Full x86 protected-native build PASS. Final DLL in
  build/menu-controller-laser-final-20261108/d3d9.dll, SHA256
  39D79E11A4B57A051E66239AAC852676E552A88FF6E8B84F1FD3A0AE9833456D.
- First pointer test compile missed <initializer_list>; added include, fresh
  final test PASS. Rebuilt DLL after final trigger-latch change.
- Runtime menu hover/confirmation and physical headset NOT RUN. Static tests
  do not prove native Flash highlight or Pico comfort. No game launched/driven.
- Ladder remains deferred by user until they locate the after-Swifty scene.

## Deployment

Pending freeze/install using tools/prepare-menu-laser-candidate.ps1, which pins
F11472BE rollback and protects ten other files including both INIs, language
helper/manifest, EXE, loader and both cache pairs. No release/push/main merge.
