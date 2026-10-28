# Protected left-controller target query

Integration: commit `a564ab3` on top of `bef0f9a` and its prerequisites, or use
`feature/render-command-worker`. Build x86 with `build.ps1 -DiagnosticCapture`.
Merge `on.diagnostic.overrides.ini` into the existing diagnostic INI; it is not
a replacement configuration. The candidate requires protected input/gestures,
head geometry, NativeViewBoundary=3 and the pinned native image.

Keep the legacy Input.NativeLeftAim, NativeWeaponPose, NativeTrackedHands,
GestureGamepadBridge and TentacleGestureBridge disabled. Render size and other
quality/performance settings do not belong to this override.

The left-controller aim pose supplies the ordinary-target query while LB is
held and neither trigger owns the input. The right controller can be untracked.
Native ownership, query filters and collisions remain authoritative. This does
not implement swing-directed throw velocity or prove physical VR alignment.

Runtime evidence: `[ProtectedPlayerOwner] status=8`, then
`[ProtectedLeftAim] private camera getter published`, then
`[ProtectedLeftAim] controller ray applied`. A requested flag alone proves
neither installation nor ray application. The CULL label is a separate route.

Rollback with the game closed: restore the prior DLL and diagnostic INI from
the deployment bundle's `backup` directory. Setting ProtectedLeftAim=0 requires
a game restart; it prevents private-table publication in that new process.
No hashed EXE or native static table is modified.

Optional swing-directed release: merge `throw-on.diagnostic.overrides.ini` and
restart. This adds a private copy of the validated fire-event/release methods.
Squeeze to acquire; after a powered stroke release the left grip to issue the
stock second LB pulse. The chosen stroke velocity and controller aim determine
direction (existing ThrowAimAssist, default0.20); native speed/spin and physics
remain. The right hand is independent. Characters/retained targets keep the
native release path. `throw-off.diagnostic.overrides.ini` disables this addition
on restart. Runtime receipts are `[ProtectedLeftThrow] owned fire event
published`, an armed `[NativeThrow] request`, then `[NativeThrow] APPLIED`.
This candidate is offline-tested; copied-call compatibility and HMD trajectory
remain unverified. No measured FPS improvement is associated with this option.

## Controller action access

During gameplay, press left grip (L1) before triangle to send stock XInput Y.
The native game selects the action associated with Y, including context actions
while holding an opponent. The other three face buttons retain A/B/X. Without
this modifier, triangle keeps the existing menu tap / hold-to-recenter behavior;
the separate menu button remains Start. Ownership is latched for the entire
triangle press: releasing grip during Y cannot pause/recenter, and gripping
after a menu press cannot turn it into Y. Focus loss or a polling gap requires
a fresh triangle release. This mapping requires no native ownership admission.

The 2026-10-28 18:29 runtime log confirms culling-cover receipts but rejects the
renderer+460 player-owner path (status0 during gameplay). ProtectedLeftAim and
ProtectedLeftThrow were requested without private publication in that run.
Their native trajectory is therefore not runtime validated; repairing the
player-owner source is separate from exposing Y. Do not loosen its type or
reciprocal-owner checks to admit an unrelated object.
