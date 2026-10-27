# Matrix capture and exact native source candidate

Base: 42f242a, feature/render-command-worker. Runtime acceptance: NOT RUN.

## CPU correction

Installed-image Ghidra evidence identifies the common four-vector matrix
submission at VS return 571E68 / PS return AD3B78. Several engine computations
write the current parameter block's fixed scratch matrix at +330 before this
callback. This extends the nine-field ownership policy with that matrix.
Recorder capture avoids ReadProcessMemory only when the existing exact native
frame, parent chain, EBX owner, owner+1C block and exact block+330/64-byte source
all match. A helper returning another pointer does not qualify. No prior matrix
is reused; engine computations and D3D9 uploads are retained.

Evidence remains private in analysis-derived/native-material-owner-abi.txt and
native-material-direct-getters.txt under the retained zero-loss capture folder.
Full producer bodies remain fingerprinted; helpers are not hooked. Earlier nominal
2.22 ms/present RecPush-inclusive exposure is an upper exposure, not a prediction
for this incremental field. Coverage and frame savings need game measurement.
Dynamic arrays, singleton returns and nested objects retain the baseline path.
State-block/concurrent mutation contracts do not yet justify live driver constant
elision, so no such cache is introduced.

## Native resolution target

SourceQuality=4 requests exactly RenderWidth/RenderHeight through the protected
external USER32 observer at native caller C96474. Configure 2676x2728 for the
requested native render target per eye. The engine receives one coherent size;
dependent resources are not independently substituted. Modes 0-3 retain their
previous behavior; invalid/excessive sizes are refused. The diagnostic protected
architecture is required; ProductionResolution remains 0.

This requests about 3.55 times the pixels of the old 1920x1071 source. It is a
quality requirement, not a CPU optimization or promised 90 FPS. Verify native
sizing requests, actual geometry/depth/output dimensions, both-eye projection
and viewport/UI behavior before acceptance. OpenXR eye dimensions and final RT0
alone are insufficient proof.

## Offline checks and integration

Actual x86 native-shaped gate/hook: 4073 checks PASS. All ten fields preserve
nonzero payload bits and reject shifted/oversized sources; option, owner
replacement, guard fallback and sticky-fault behavior remain covered.
Real external trampoline fixtures Quality and Exact2676x2728 PASS, including
BOOL/last-error, caller/window restrictions, unchanged EXE text and size bounds.
Offline 64-byte gate/stub timing: RPM 644.8 ns/call, owned copy 35.2 ns/call.
This is not driver/game/FPS evidence.

Integrate after 42f242a. Keep prior requested CPU package features enabled,
NativeOwnedConstantCopy=1 and NativeOwnedMaterialCopy=1. L3+R3 toggles the CPU
package; Triangle hold recenters. SourceQuality requires restart and does not
toggle with the CPU package. Preserve a DLL/profile rollback.
