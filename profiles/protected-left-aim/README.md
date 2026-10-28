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

Runtime evidence: `[ProtectedViewOwner] status=8 accepted=1` (or the older
`[ProtectedPlayerOwner] status=8` fallback), then
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

During a native-confirmed character/retained-target hold, triangle directly
sends stock XInput Y; no extra grip press or valid controller pose is required.
Outside that confirmed context, press left grip (L1) before triangle to send Y.
The native game selects the action associated with Y, including context actions
while holding an opponent. The other three face buttons retain A/B/X. Without
this modifier, triangle keeps the existing menu tap / hold-to-recenter behavior;
the separate menu button remains Start. Ownership is latched for the entire
triangle press: releasing grip during Y cannot pause/recenter, and gripping
after a menu press cannot turn it into Y. Focus loss or a polling gap requires
a fresh triangle release. The L1 modifier needs no native ownership admission;
automatic held-target context requires fresh gameplay feedback and the owned
private camera. A prop hold, stale/missing owner, menu or absent view does not
enable the automatic action context.

The 2026-10-28 18:29 runtime log confirms culling-cover receipts but rejects the
renderer+460 player-owner path (status0 during gameplay). ProtectedLeftAim and
ProtectedLeftThrow were requested without private publication in that run.
Their native trajectory is therefore not runtime validated; repairing the
player-owner source is separate from exposing Y. Do not loosen its type or
reciprocal-owner checks to admit an unrelated object.

The next candidate also observes the result of the renderer's existing native
virtual+174 call at985F75. It forwards that call once inside the digest-pinned
private985D80 copy, while the native out-reference is still live. Only a result
passing the complete D2Avatar/camera/inventory reciprocal graph can publish
the private aim/throw tables. No additional native getter is invoked and no
renderer+460 FOV override is required by this route. The adapter replays the
displaced native pointer load and comparison, including flags. This is an
offline-tested candidate: an actual accepted result and controller trajectory
still require runtime receipts. Tracked visual hand/bone poses are not added.
