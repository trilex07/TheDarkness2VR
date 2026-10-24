# Native camera / FOV / visibility audit

2026-10-24. Task branch starts at research commit `cb6638f`.

## Gate status

Gate A is **incomplete**. A perspective-matrix primitive and an eight-corner
frustum primitive are now identified, but the active player-camera projection
and CPU visibility-plane producer/consumer have not been established together.
The earlier source comments naming
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
Java-side reference analysis remained available. In the later follow-up the
decompiler worked again, producing private output for the three explicitly
asserted entries below. No protection settings or quarantined files were
changed to obtain this result. Ghidra's auto-inferred register signatures are
not a verified native hook ABI.

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

## Follow-up: units, receivers and geometric producers

The depot sample's Lua bindings establish **different type lookup globals**:
`GetFOV` loads `[01100984]` at `00DC478F`, whereas the setter loads
`[01100994]` at `00DBDBF2`. The latter global is associated with the literal
`CameraControllerBase` through descriptor `010B93D0` (name at `010B93D8`,
type-global pointer at `010B93DC`, binding array at `010B93E8`). The getter's
descriptor is `010B99E0`, type-global pointer at `010B99EC`, binding array at
`010B99F8`; its class-name bytes are zero in this depot sample.

This does **not** prove the receivers are unrelated: inheritance or an explicit
conversion could connect them. It does prove that the existing shared
`BaseAvatar` assertion in the source is insufficient. A receiver cached by
GetFOV slot 270 cannot be certified for setter slot E4 from these bindings.
The concrete field accessor pair above is not a substitute for that proof.

### FCOS path

`007BF850` dispatches slot 270; `007BF858` multiplies ST(0) by 0.5;
`007BF865` computes FCOS; `007BF870` stores the result at `[ESP+44]`.
At `007BFAEF..007BFB08`, the routine passes vectors to `00B86190`, then
compares its result against that stored cosine and can reject a candidate.
`00B86190..00B861C0` is independently a three-component dot product:
multiply packed components, sum x/y/z, return ST(0), `ret 4`.
The loop has entity/type tests and scalar-distance selection; it does not
write a perspective matrix in this path. Calling this a projection hook is
incorrect.

FCOS consumes **radians**. Thus this consumer evaluates `cos(raw_slot270/2)`
with a radian contract and no degree conversion in between. That establishes
what the consumer computes; without the concrete getter and its producers it
does not prove the getter's documented units, nor that it supplies the camera's
FOV. Camera-like degree ranges (30,70) remain an unjustified receiver heuristic.

### Perspective matrix primitive

`00A31FF0..00A320B6` is a native perspective-shaped 4x4 matrix constructor,
identified from all sixteen output components, independently of GetFOV sites.
Ghidra and Capstone agree on its entry and stores. Original entry bytes:
`55 8B EC 83 E4 F0 83 EC 0C`. Output is ECX (saved to ESI); EAX returns it;
`ret 14` pops five stack arguments. The first stack argument is unused here.
The remaining arguments are angle, aspect divisor, near and far.

Let `T(x)` denote the scalar SSE math routine `00E1BC92` (its exact CRT symbol
and numerical equivalence to tangent have not been independently validated).
The stored matrix, in linear row groups, is:

```
s/aspect  0  0  0
0        s  0  0
0        0  q  1
0        0 -q*near 0
s = 1 / T(angle * 0.008726646192371845)
q = far / (far - near)
```

The half-angle degree conversion and perspective depth structure are explicit;
do not silently convert this into proof of the live camera's final FOV axis.
Direct caller sites verified in Ghidra are `0044609F` (function `00445E40`),
`00467737` (`004675F0`), `008D3F47` (`008D3EA0`), `00BA919E` (`00BA9150`).
At `0044608B..0044609F`, an object's fields `+184` and `+17C` supply aspect
and angle respectively; the output is then passed to matrix multiplication
`00C02F50` at `004460BC`. Which of these callers supplies the player's
recorded world projection remains open.

`00DF3B50..00DF3C00` is a second perspective-shaped constructor with the same
depth pattern, output in ESI and far in XMM0. Entry bytes:
`55 8B EC 83 E4 F0 83 EC 10`. Caller `0096ED99`, inside `0096E4F0`, copies
the matrix into an object's `+480`. This **custom register ABI** is not a
normal C++ thiscall hook signature and has not been runtime-validated.

### Frustum geometry is not yet CPU visibility proof

`00DF53E0..00DF5596`, entry `55 8B EC 83 E4 F0 83 EC 70`, builds **eight
world-space corner vectors**, not six clipping planes. It calls rigid inverse
`00D46710`, reads angle/aspect/near/far arguments and writes eight 16-byte
vectors through ESI at offsets 0..70, using combinations of transformed basis
vectors. Direct callers are `0070CB42` (function `0070CA40`), `00C6DC1D`
and `00C6DE9A` (both in `00C6DAD0`). At `0070CB28..0070CB42`, object fields
`+17C` and `+184` feed angle/aspect; at `0070CB61..0070CB79`, the caller
reduces all eight corners to componentwise min/max bounds. That proves a
geometric bounds consumer, **not** the main CPU object-visibility decision.

The next static gate is to connect a concrete active-camera receiver to its
projection output and the actual visibility plane/test consumer. The next
live gate additionally requires the safely restored, re-audited installation.

## Reproducible offline tools and disabled experiment

The previously missing `audit_camera.py` and `test_audit_camera.py` were present
at the start of this follow-up and are now included in version control.
They are read-only evidence tools using Python's standard library:

```
python -m unittest discover -s tools/native-camera -p "test_*.py" -v
python tools/native-camera/audit_camera.py <restored-installed-DarknessII.exe>
```

Six parser tests pass. They cover truncation, PE32 validation, VA/raw mapping,
section crossing, zero-fill and matching-header/unknown-hash rejection.
The depot sample reports six matching instruction sites but `known_identity`
**false**, exit code 3: the tool deliberately accepts only the previously
measured installed SHA, not the newly downloaded variant. Matching bytes
never overrides the hash gate. A successful audit is necessary evidence for
a proposed patch, not authorization or proof of its receiver or semantics.
No native patch is proposed or deployed in this follow-up.

Attempts to edit those two existing scripts failed with `Permission denied`,
including an elevated attempt. Their ACL grants the sandbox read/execute.
They were left intact; no ACL or antivirus changes were made. Their present
identity policy must be reviewed after the user resolves the detection.

`CameraEvidence.java` now accepts `VA:HEX` for independently verified function
entries, checks instruction bytes and executable-section membership, rejects
entries inside another analyzed function, and only then disassembles/creates
the function in the **private Ghidra project**. Plain VAs never invent entries.
The private producer export completed with all three assertions matching.
Headless Ghidra can exit zero after a script exception, so the script now emits
`EVIDENCE COMPLETE` only after all queries finish. Check that marker and the
absence of script errors, not just the process exit code.
Follow-up validation passed: all three original signatures produced the final
marker; an intentionally incorrect byte in the first entry was rejected and
produced neither an asserted-entry record nor a completion marker.

`tools/native-camera/runtime-experiment.json` prepares an **observe-only**
runtime sequence, with experiment, deployment and native patching all disabled.
It is not read by the mod and changes no INI. It requires re-auditing the
restored installed EXE, preserving deployment hashes, and correlating frame
IDs/projection/visibility/draws before any native-wide experiment. It does not
implement an unverified hook or certify the missing visibility producer.
