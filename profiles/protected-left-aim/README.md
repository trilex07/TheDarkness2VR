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
