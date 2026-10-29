# Headset feedback follow-up, 2026-10-29

Baseline deployed proxy SHA-256:
`2C8F5AF7D5C798B56A636F79E7085D58A3181D136358D15F300941A24168285E`
(a508d5b5 source receipt). Preserve DirectRecordCopy=0 and all accepted INIs.

## L2 and peripheral disappearance

The retained game log contains NativeCullingRefusal with witnessReady=0 during
FOV changes (including frame 7436); later records show FOV 42.031 and 45 versus
60 in ordinary play. This supports a projection-transition defect, but does
not timestamp the user's L2 press or establish that every artifact has this cause.

The actual selected-camera callback required both the previous frame's exact
projection and the currently retained shader inverse to equal the new native
camera projection. A changing ADS FOV cannot satisfy that order: the new shader
upload follows the culling copy. Coverage was consequently disabled during
the transition even for the already admitted primary camera.

Keep the immediately preceding corroborated camera/view/renderer identity,
known projection, pinned before-copy boundary and current native camera checks.
Construct culling from the current native view/projection/VP and exact frame eye
transforms, whose consistency/singularity/finite/depth checks remain mandatory.
Do not require a previous frame's numeric projection to be unchanged. Keep the
native volume and widen only; shader corroboration continues unchanged for the
next frame's receipt. No camera projection, render matrix or EXE bytes change.

The actual-source boundary fixture now exercises 20 consecutive zoom frames
with a fresh matching shader upload after each boundary; different cameras,
missing receipts, unknown projection and prepare/rebuild failures still refuse.
Binocular geometry and native rebuild ABI regressions remain required.

## Hands in cinematics

Still open. A proposed camera equality gate was removed before deployment:
render-selected PerspectiveCamera (FF2800) and the avatar controller camera
(FFC7C0/private instance) are different objects even during normal gameplay.
A synthetic fixture that made those identities interchangeable was inadequate.
Do not hide hands from that comparison or from the render-activity flag.
No hands/weapon/tentacle changes are included in this candidate. A native
cinematic-state or active-camera producer contract is still required.

## Haptics and validation

The helper's one-second idle disconnect is reproduced with the previous binary
and a fake driver; both fixed IPC paths retain effects through 1600 ms silence
without heartbeats or redundant submissions. This fixes resistance lifetime,
not the unconfirmed grip vibration delivery. Bounded OpenXR result/binding
receipts are included for the next consolidated headset session.

Offline tests are not physical vibration, peripheral visibility or FPS proof.
