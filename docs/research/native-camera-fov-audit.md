# Native camera / FOV / visibility audit

2026-10-24. Task branch starts at research commit `cb6638f`.

## Gate status

Gate A is **incomplete**. No native projection builder or CPU visibility-plane
builder has yet been positively identified. The earlier source comments naming
the candidate calls as projection/culling are not evidence of those roles.
Gate B's native widening experiment must therefore remain unimplemented; Gate C
has not been run. There is no newly reproduced peripheral-disappearance claim.

The previous stationary PSVR2 fusion result belongs to the base branch and does
not validate native-wide FOV, moving-head cameras or CPU visibility coverage.

## Executable identities

- Previously installed executable: SHA256
  `55C8D634FCBE2996D8397D15D68EA728C999697F1DD40E8779B5AFE8BBB4C339`,
  14,291,512 bytes; measured before it disappeared during this investigation.
- Newly downloaded Steam depot 67380 file: SHA256
  `B56B59981FBFC03CD6A9332E7E4B12DD350DA77D4DF3CD541FBF6CDF40F6CEFA`,
  14,285,824 bytes. It remains a distinct executable variant, not an assumed
  byte-for-byte replacement for the previous installation.
- Both inspected files: PE32 x86, timestamp `4F68A875`, preferred image base
  `00400000`, image size `00DEC000`. All six instruction sites below also match
  in the newly downloaded file. Matching PE headers alone are insufficient.

No game binaries, extracted assets, decompilation or raw private traces belong
in this repository.

## Candidate map: what the instructions actually establish

- `00DC4740`: Lua `GetFOV` binding. At `00DC47BC`, bytes
  `8B 82 70 02 00 00 FF D0` dispatch through receiver ECX, vtable byte offset
  `270`. The float result is stored on the stack and passed to Lua at `DC47C9`.
  This proves the binding dispatch, not the concrete native implementation.
- `00DBDB90`: Lua `SetBaseFovOverride` binding, **not** a native avatar method.
  At `00DBDC2B`, bytes `8B 82 E4 00 00 00 83 C4 04 FF D0` dispatch through
  vtable byte offset `E4`, with receiver ECX and a float stack argument.
  The concrete method and the receiver's verified runtime type remain open.
- `007BF850`: bytes `8B 82 70 02 00 00 FF D0 D8 0D 08 BA FF 00`.
  The returned float is multiplied by `[00FFBA08]`, followed by **`FCOS` at
  `007BF865`**, not tangent or a local perspective-matrix construction.
  Its downstream gameplay/visibility role is unresolved. It must not be called
  the projection builder solely because it reads an FOV-like virtual slot.
- `0076968A`: bytes `8B 82 70 02 00 00 FF D0 D9 EE DF F1 DD D8`.
  The immediate consumer tests the float against zero and branches at `769698`.
  This does not establish CPU plane construction.
- `00B23F7F`: bytes `8B 90 70 02 00 00 8B CE FF D2 D8 64 24 08`.
  Containing routine starts at `00B23F30`, sums float modifiers through
  `00511D90`, calls `00B880E0`, optionally calls slot `270` with ECX=ESI, then
  subtracts a scalar and returns with `ret 4`. No plane stores are established
  by this local sequence. The name `InstallFrustumGetFovHook` is unproven.
- `006CFCB0`: returns the float at `00F90DAC`. That constant has numerous
  unrelated references; its value `90.0` does not identify a mutable global
  camera setting. Neither this constant nor `[00FFBA08]` was changed here.
- `00C02F50`: SSE matrix multiplication; `00D46710`: rigid-transform inverse
  pattern (rotation transpose and negated transformed translation). Neither
  identifies the native perspective/frustum producer by itself.
- `00A94052`: `8B 41 0C FF D0`, the separately verified engine presentation
  boundary. Its implementation is unchanged in this worktree.

The legacy degrees assumption is also unproven: `GetFOV() * 0.5` reaches x87
`fcos` without a visible degrees-to-radians conversion in that sequence.
Radians are a lead to verify, not a certified ABI/unit contract.

## Additional static evidence from the Steam depot sample

The following observations are specific to the `B56B5998...` sample. They are
not permission to use these addresses in the different installed executable.

- The property descriptor passed at `004B3837` is `010B536C`.
  Its layout is **flags, name pointer, description pointer**: the description
  at `010B5374` points to `Base FOV, leave at 0.0 to use default gFOV`.
  Serialization immediately afterward passes `owner + 88C` to virtual slot
  `74` (`004B3845..004B384E`). Do not shift the descriptor by eight bytes:
  that would incorrectly associate the adjacent bank-clamp pair with FOV.
- `007BD4B0`, bytes `D9 81 8C 08 00 00 C3`, reads the scalar at `ECX+88C`
  into x87 ST(0), then returns. `005F9D20`, bytes
  `F3 0F 10 44 24 04 F3 0F 11 81 8C 08 00 00 C2 04 00`, writes a float
  stack argument to the same field and returns with `ret 4`. These establish
  a concrete base-field accessor pair; **they do not yet identify the method
  reached by the Lua GetFOV dispatch at slot 270**. Static pointer references
  exist at `00FDF254` (setter) and `00FDF258` (getter), but the owning vtable
  start and receiver hierarchy still require proof.
- `00AFB730` selects its nonzero float argument, otherwise `[ESI+88C]`,
  otherwise `[010E3268]` (`00AFB757..00AFB776`). Later branches divide or
  multiply the selected value. This proves a fallback consumer, not a
  projection/frustum builder or the final camera's units.
- In this sample `[00FABB3C] = 0.01745329238474369` (degrees to radians),
  `[00FB2E10] = 0.008726646192371845` (half-angle conversion), and
  `[00FFBA08] = 0.5`. At `006BF441..006BF473`, `[EDI+17C]` is multiplied by
  the half-angle conversion and passed to `00E1BC92`; a separate x87 `fptan`
  path follows. The surrounding routine also computes bounds distance and
  model-related scalar values. Its trigonometry alone does not establish a
  matrix or CPU plane producer.

Ghidra's partial analysis places `007BF850` inside `007BF6D0` and `0076968A`
inside `00768D10`. The analysis hit its 240-second limit and saved its project;
it was **not** a complete executable analysis. Unanalyzed candidates must not
be treated as absent functions. Capstone independently decoded the explicit
accessor entries and descriptor-consumer sequences above; no host execution
or native hook was used to obtain them.

## Source-level findings, separate from runtime proof

`TryNativeBaseFovOverride` returns immediately for `SubmitMode=4`. Therefore
the presence of this function is not evidence of a widened native camera in
the working same-frame stereo mode.

`MaybeInstallFovProbeAfterDevice` respects `StartupMode>=2`, but the later
`TryApplyEngineFovOverride` path can still install probes through
`InstallGetFovProbe`. The existing diagnostic-startup setting does not prove
absence of all native FOV patches. Historical logs actually contain installation
messages in this setup. No claim of successful native widening follows from
an installation message.

`NoteAvatarFromGetFov` accepts a value range (30,70) plus an executable-data
vtable range. That does not prove a BaseAvatar receiver, ownership or lifetime.
It must not justify calling a cached receiver's setter.

`ComputeHmdCoverEngineFovDeg` labels `Px/Py` as aspect. For an ordinary symmetric
perspective projection, aspect is `Py/Px`. The engine's actual FOV axis and
units must be proved before replacing its computation.

The existing GPU widening/metadata path remains intact. A future native camera
change must measure the **new** projection and derive only residual GPU scaling
from it; disabling GPU widening merely because a setter was called does not
prove matching projection, CPU planes or OpenXR rays in that game frame.

## Required runtime witness

One frame witness must include the real native camera receiver and lifetime,
original/effective FOV with established units, native projection output and
the CPU visibility frustum derived from that camera, engine frame/present ID,
recorded draw/primitive counts, and the actual pair's matrix scales/crop and
submitted FOV. Merely logging an FOV read or counting more draws is insufficient.

If disappearance is reproduced, hold scene/pose fixed for a baseline/wide pair.
Locate the affected object's draw: absent recording requires distinguishing
CPU frustum from occlusion/LOD; a recorded draw requires examining clip-space
coordinates, depth/clip state and later passes. No culling-disable shortcut is
justified before that discrimination.

## Tooling and environment

`tools/native-camera/CameraEvidence.java` exports Ghidra's analyzed function
boundaries, references, instructions and (when available) decompilation to an
explicit **private output path outside the repo**. It does not invent a
function entry at an arbitrary interior call site. Decompiled output must stay
private. The script compiles against official Ghidra 12.2's Java API.

The official Ghidra 12.2 ZIP was checked against release SHA256
`2C2CD776DB3C1F6A097BA418922378AC3649CADD5E844A6F5F088AE628A7BE67`.
The private project is outside the repository. Windows MSIX redirected AppData
initially broke Felix's canonical-path comparison; a private
`-Dapplication.settingsdir=...` outside AppData resolved initialization.
The native `decompile.exe` subsequently became unavailable; disassembly and
Java-side reference analysis remain available.

User confirmed Bitdefender quarantined the installed game executable. No
quarantine restore, exclusion or protection-setting change was performed.
Computer Use fails with `codex-computer-use.exe ENOENT`, including after reset.
At the user's request, Steam was invoked with `steam://validate/67370`.
Steam's own console logged `VerifyApp -> VerifyingFiles`; content log records
depot downloads, then at **18:27:21 Europe/Warsaw** a commit failure moving
`DarknessII.exe` into the game directory: **Disk write failure**. The log does
not establish the cause of that failure. Verification therefore did not
repair the installation; the game was not launched.

Offline rendered-FOV regression: **PASS, 1206 rays** on 2026-10-24. This is
unchanged production math coverage, not native-wide or headset validation.
