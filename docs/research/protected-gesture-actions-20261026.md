# Protected stock gesture actions

Base `c2e2304`. IMPLEMENTED / OFFLINE VERIFIED; no Steam or PSVR2 test.

`Diagnostics/ProtectedGestureActions=1` is OFF by default and requires the
protected Present, input bridge and image-write guard. It keeps both legacy
gesture flags and every native aim/throw/pose hook disabled.

The existing face/hip dwell logic now sends the game's objective/reload
buttons in this separate mode. The existing pure `Tentacles` right-stroke
state machine supplies RB followed by directional right-stick input. Raw
right-grip RB is suppressed until a valid stroke; left-grip LB is retained.
No protected action calls `ReadNativeGrabFeedback`, `PublishNativeAim`,
`PublishNativeThrow`, `PublishNativeWeaponPoses` or the legacy native bridge.

Pause is latched from the stock Start action; focus, tracking loss and sample
discontinuity cancel pending strokes and require rearming. The known
render-activity heuristic remains the gameplay-context source; native pause
changes made through other input devices are not newly identified here.

This restores button-level gestures only. Controller-aligned selection,
throw direction, tracked meshes and weapon support are not restored by it.
It adds no EXE writes, guessed vtables or graphics changes. The new profile is
separate; the previous protected input profile and installed files are intact.

Tests: `build-protected-gestures.ps1` compiles actual gesture and quaternion
bodies and checks 227 assertions covering grip-only suppression, directional
pulse, release/rearm, focus/tracking loss, pause/resume, face/hip dwell and OFF
behavior. Its source guard rejects calls into native hooks. Profile tests and
production/diagnostic builds also pass. Runtime/HMD acceptance remains pending.
