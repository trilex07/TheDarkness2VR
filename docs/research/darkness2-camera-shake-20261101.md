# Darkness II VR camera-shake source audit (2026-11-01)

Pinned stock `DarknessII.exe` SHA256: `D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1`. Static addresses below are image VAs. No installed game file was modified for this audit.

## Confirmed producers

- Extracted scripts `D2/Scripts/CameraShake.lua` and `Audio/DarknessVoice.lua` write `postProcess.viewShake.mShakeAmbient` repeatedly.
- `Player/Stoned.lua` writes ambient, dampening, position/rotation factors, speed and sway fields. `PostProcess/DrunkWalk.lua` writes ambient, rotation factors and speed.
- Registered `mShakeAmbient` setter at `00DC8F50` resolves the script target via `00DADA80` and writes a float at object `+0x2C` (`00DC8FFC`); getter `00DC9030` reads the same field (`00DC90AA`). These functions cover one script property, not all native camera motion.

## Native candidates and limits

- The executable also contains named parameters `mCameraShakeStrength`, `mPlayerShakeStrength`, `mRunShake`, `mWalkShake`, `mImpactShake`, `mFallScreenShakeScaling`, `mRecoilStrength`, `mFPEntityIgnoresShakeTranslationAndRotation` and others. Their names do not establish a shared camera transform or safe live offsets.
- The `mFPEntityIgnoresShakeTranslationAndRotation` name at `00FB8900` is a property metadata reference at `010B534C`, beside its description at `010B5350`. It does not identify an owned live first-person entity or an effective on/off value.
- A read-only scan for scalar loads at offsets `+0x2C`, `+0x34`, `+0x44`, `+0x48` found a cluster at `008D2662..008D2715`. It interpolates many fields between two objects, so it is not established as the camera-shake producer. `0066BC76..0066BD24` copies matrix rows; `00BAD219..00BAD260` reads camera-like parameters, but neither has proven `viewShake` ownership.

## Candidate gate

Identify the live owner and the native transform application order for camera, first-person arms, and gun. Suppress shake upstream of all three; preserve look, locomotion, aiming, recoil feedback that does not move the head, and cinematic transitions. A camera-only correction is excluded because it could leave the hands and weapon oscillating against a still scene. No shake-disable patch is ready or installed.
