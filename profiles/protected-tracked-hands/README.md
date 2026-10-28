# Independent human hand candidate

Merge the diagnostic override into an existing protected native stereo profile,
then restart. Requires ImagePreservingInput, ProtectedGestureActions,
ProtectedHeadTracking, ProtectedLeftAim and Render.NativeViewBoundary=3.
Legacy NativeTrackedHands, NativeWeaponPose and NativeHandsPackHook remain OFF.

The live selected-avatar reference can publish a private96-method resource
table. Only its +EC skin packer entry differs. Both grip spaces publish
independent poses; neither requires a pistol, aiming pose, support grip or the
other controller. Each successful IK solve writes only its own human arm.
Busy/alternate native holds retain the engine animation. Existing aim/throw
physics and contextual triangle/Y mapping remain separate.

Rollback: merge off.diagnostic.overrides.ini and restart. No native EXE,
IAT/static class table or debug registers are modified. The original native
packer retains its own renderer-reference consumption and all side effects.

Offline tests establish graph/ABI/pose contracts. Visible human mesh consumption,
wrist calibration, interaction and FPS require runtime verification. A log of
CPU palette writes alone does not establish visible independent hands.
