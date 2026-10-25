# Darkness II VR: hands and enemy-grab root cause

Status: investigation in progress on `codex/root-cause-hands-grab`, based on
`bf3a860`. No candidate from this branch has been installed in Steam. The
visually accepted head-following tentacles and independent world geometry are
preserved unchanged.

## Evidence identity and private manifest

- Installed EXE: PE32, image base `00400000`, timestamp `4F68A875`, SHA-256
  `d46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1`.
  Every VA below is specific to this image.
- Last enemy-grab trial log: private
  `%TEMP%/d2vr-scoped-trial-20261025-190821/trial-d3d9.log`, SHA-256
  `9fbf6b894a347356c3b5b297c8cc6d46b38868aac9a7c835fd260e93a5b3380b`.
- Earlier pistol/IK trial log: private
  `%TEMP%/d2vr-scaled-trial-20261025-185316/trial-d3d9.log`, SHA-256
  `cdaf38ffbf59ea624cf141e1793d2f2a4c9484159118996bc4a4f778340d0a9f`.
- Existing matched graphics capture manifest: private
  `%TEMP%/Darkness2VR-matched-36500-1-38224/manifest.jsonl`, SHA-256
  `97f23db40adc7f7cb03e4d821364b5fac5034a91be2f4fcb547734a4013a8a34`.
  Its header states `native_hooks_suppressed=true`; it can identify stock draws,
  but cannot prove this hand writer's effect.
- A second private D3D9 capture is
  `%TEMP%/Darkness2VR-matched-37812-1-25308/manifest.jsonl`, SHA-256
  `74abee85dfce6d003558627e41d51f473bca0e16388fdf3aae4f6211b2003a69`.
  Its DLL SHA-256 is
  `64b1c40feb4af6ab1cfa2ba6734ca57d083f52160e006242ceeab5f3908aef9f`:
  the earlier `bc47dab` peripheral-camera candidate, before the accepted
  hand/tentacle trial. `native_hooks_suppressed=false`, but the capture is
  **incomplete** (mask 3, no final D3D11 eyes or `COMPLETE.json`). Its two
  source images and D3D9 draw banks are usable as limited historical evidence,
  not as a matched final-eye capture or a test of the current hand writer.
- Ghidra projects with SHA `55c8...` are an older executable and are excluded
  from current-address conclusions. The current private Ghidra project is
  `%TEMP%/d2vr-rootcause-ghidra/RootCause.gpr`; its exported evidence files
  `d2vr-rootcause-current-exe.txt`, `d2vr-rootcause-followup.txt`,
  `d2vr-rootcause-upload.txt`, `d2vr-rootcause-next.txt`,
  `d2vr-rootcause-pose.txt`, and `d2vr-rootcause-render-owner.txt` identify the
  pinned `d46bb6...` EXE and end with `EVIDENCE COMPLETE`. SHA-256 for the latter
  three exports, respectively: `b1fed8159e6f48743aad3937ef2338c6035829c97a68359fcae84ec164e3df93`,
  `36876a218164dbba6230563e649702c87ce43d02b8d340bf950fd1954cdc50a5`,
  `89c845123d717563cd3fb13f7c0027e3a0db1dcaab3b27d43ced0bd79bfd3c2e`.
  `CameraEvidence.java` accepts explicit addresses without inventing function
  entries. `RootCauseRefs.java` requires the exact EXE SHA, and its private
  `d2vr-rootcause-refs-verified.txt` export has SHA-256
  `7a028de0aaef910d33739c2eea6dcb558dd026327f878ac8081c602588dff10c`.
  Reproduce with the existing `RootCause` Ghidra project and the committed
  scripts in `tools/native-camera`, then check each export's identity and final
  marker. No executable, dump, extracted asset, or private trace is committed.

## Hands do not follow controllers

**Symptom/reproduction.** In the latest PSVR2 gameplay trial the pistol could
follow the right controller, while the human arm/hand model remained centered.
The trial log shows no `NativeHands` application. A prior trial with the pistol
equipped logged repeated `NativeHands ... applied` messages while the user still
saw stationary arms. These are two distinct failures.

**Confirmed call path.** `NativeHandsFromWeapon` is called only inside the
successful `TryNativeWeaponMatrix` branch in `NativeWeaponCompareChecked`
(`src/native_weapon.inl`). `ApplyNativeHandsChecked` also requires
`sample.entities[0]`, a verified pistol model. The latest trial had no
`[NativeWeapon] active pistol model` or pose-application line. Thus zero hand
applications in that trial are explained by the pistol-only call gate. This
does not explain the earlier *visible* failure after successful CPU writes.

**Engine/disassembly evidence.** Current-EXE Ghidra shows `007990B0` modifying
the selected pose vector at renderer `+3F4+16*poseSlot` and setting
`+580+skinSlot` dirty at `0079927D`. The resource virtual method `006C3890`
(entry stored at `00FC2664`, `+EC` from the verified resource vtable
`00FC2578`) selects the three-slot pose vector via renderer `+424`, tests the
two-slot skin dirty flag via `+448`, then at `006C3DE6..006C3E20` maps bone
indices through `+5A8` and calls `00B1E500`. The latter composes the pose and
inverse-bind matrices into 48-byte GPU-layout rows in renderer `+428`. A second
renderer method, `0066B360`, can copy an existing packed vector from `+570` to
`+428` and clear the same dirty flag at `0066B4B6..0066B4C4`. Therefore a
dirty byte alone is insufficient: the engine may overwrite the output after
the mod's edit. `00883C70` rotates the renderer's three pose slots (`+424`)
and two packed-skin slots (`+448`) before calling `0066B360`; its caller
`009962E0` continues model work afterward. This confirms that the slots can
change independently between updates. It does **not** establish whether the
pistol comparison hook at `00AD94A2` fires before or after the final
`006C3890` pack and VS upload for the visible arms. `006DCBB0` and `00CF2510`
also contain these offsets, but Ghidra shows they construct and copy renderer
objects; they are not GPU-upload evidence. The first-person pack-to-draw
ordering and final VS upload remain unproven.

An exact-byte assertion on the current EXE confirms `00B74720` is a thiscall
wrapper around the D3D9 device's `SetVertexShaderConstantF` vtable slot
`+178`; it takes the device from receiver `+1A4` and returns with `ret 0C`.
`008F8150` invokes the VS semantic dispatcher `0071F690` before other render
work. Neither fact links the avatar resource's `006C3890` pack to the live
`c45` upload: `00B74720` has only a vtable data xref and the shader dispatcher
does not name the owning avatar. The new private exports
`d2vr-rootcause-skin-upload.txt`, `d2vr-rootcause-skin-upload-2.txt`,
`d2vr-rootcause-b74720.txt`, and `d2vr-rootcause-render-flow.txt` retain the
current-EXE hash and `EVIDENCE COMPLETE`. They rule out a claimed ordering
based solely on matching field offsets. Their SHA-256 values, in the same
order, are `1e742de502ca80f28f9d3aa9a03dcc5a47bee4519153b1ed637f519b981f589a`,
`5dd53979a51f3744e691369a9e90cafac7d3e0ab0f88158cc31931398a545db8`,
`48029e22ccc8db8109b890a6c1017c1ed6a3073a152a93bbe5d0a4a34072484f`,
and `06918b6c886a7eec3832c8a26140f43dc7e5cd258a9d6ba93e882e19418ae730`.
The targeted read-only `CameraEvidence.java` export
`%TEMP%/d2vr-rootcause-pose-callers-2.txt` (SHA-256
`3dedb6dbc53aedb2d31316072349e85f0b481deb401dada11bfbf22c84a58696`)
contains the same EXE identity and `EVIDENCE COMPLETE`. It confirms three
direct callers of pose setter `007990B0`: `004EA5D1`, `0089DC2D`, and
`00B73D38`. `0089D110` loops through mapped bones, passing renderer, bone
index, and aligned matrix to the setter; `004EA2D0` subsequently reads that
renderer's selected `+3F4` pose vector. This connects a native animation write
to the selected CPU pose and dirty flag, but does not prove which caller updates
the first-person avatar or when that pose reaches its visible D3D9 draw.

**Matched draw evidence.** In the private stock capture, draw 11/12 in eye 0
share a 114,400-byte VB and 85,818-byte IB at 40-byte stride in the
first-person viewport; their VS hashes are `C745D63B` and `1CB7CEE2`. The
saved VB has nonzero skin weights with local indices 0..33. Draw 13 uses a
distinct 124,440-byte VB and local indices 0..4. Draws 0..6 use the known
tentacle VB and local indices 0..24. The first-person image visibly contains
the human arms and pistol. Draws 11/12 are thus **human-arm candidates**, and
draw 13 is a pistol candidate; no draw-isolation result or CPU-renderer/GPU-bank
identity yet proves the precise object owner. The shader consumes c45-based
skin matrices, but the capture has `native_hooks_suppressed=true` and cannot
show whether the modified CPU palette reaches those constants.

The separate historical `bc47dab` source capture shows the same candidate
mesh dimensions (VB 114,400 bytes, IB 85,818 bytes, stride 40) at eye-0 draws
47/48 and eye-1 draws 47/48, with the same respective VS hashes `C745D63B`
and `1CB7CEE2`. For each paired draw, all 816 floats in `c45..c248` are
bit-identical between eyes; their per-draw skin-bank SHA-256 prefixes are
`a2818b38a7afcfbc7065` and `9088a6afa86822733146`. The complete stock
capture likewise has bit-identical eye pairs for draws 11/12. This rules out
loss of those sampled skin constants **during stereo replay in these two
frames**. It does not establish that a controller pose entered the constants:
the historical candidate predates the hand writer and contains no CPU-palette
witness. It also does not identify either draw as a specific left/right hand.

The existing writer edits bones 2..45 in a 130-matrix CPU pose vector and
marks one or both skin slots dirty. That does not establish GPU consumption.
The next bounded witness must compare the selected avatar renderer's packed
matrices with the same frame's hand-candidate VS constants and record whether
the pose survives animation before making a new hook or offset change.

**Prepared witness, not a correction.** The opt-in matched capture now emits
`avatar_palette_overlap` only for the captured arm-mesh candidates. It records
the selected avatar renderer, pose and skin slots, dirty byte, controller-pose
write stamp, pose survival at draw time, and distinct nonidentity matches
between **both** independently rotating packed CPU skin slots and the D3D9
c45 skin bank. Both slots are checked so one rotated slot cannot create a
false negative for the capture. The stamp only
counts as a same-frame survival check when renderer, palette pointer, pose slot
and XR frame all match; a rotated slot or a stale gameplay sample is explicitly
reported. Exact matrix overlap from multiple distinct bones is supporting
evidence, while zero overlap is inconclusive because layout/transpose and
resource ownership still need confirmation. This code runs only in a requested
diagnostic capture and never changes the GPU bank. The regular production build
does not include it. Both x86 builds pass offline, and the capture validator
rejects a mixed-frame witness. It also rejects a witness with an unknown draw
identity or one recorded before its draw. The focused validator suite passes
27 tests. The diagnostic has not run in the game and cannot yet classify the
visible hand failure.

**Rejected/pending.** Replacing the pistol gate with an arbitrary camera
callback was considered and reverted before this branch. It would not prove
correct timing or GPU consumption. Prior broad palette hooks caused startup
regressions. The existing recenter observer can resolve the avatar without a
pistol, but it runs on a button event and is read-only; it is not an animation
or render synchronization point. No new palette writer or hook has been
installed here. A hand correction requires a same-frame witness showing whether
the CPU edit survives, whether the avatar's packed matrices match the live arm
draw's c45 bank, and which verified native callback precedes that upload.

## Crash while lifting an enemy

**Symptom/reproduction.** The user confirmed the game crashed while lifting an
enemy in the latest trial. The process ended and the preserved DLL/INI baseline
was restored. This physical report establishes an actual failed play session;
the vectored exception record by itself is first-chance data.

**Confirmed fault.** Last recorded `C0000005` is a read from `DD91AD64` at
`00E129EA` (`rep movsd`, EXE RVA `A129EA`). The captured EBP chain supports
three return addresses: `004E0E9A` -> `00D163C1` -> `00E37C39`. Current-EXE
Ghidra identifies `004E0E40` as a 64-byte block hash update and `00D163B0`
as its wrapper; `00E37C10` hashes a buffer while adjusting relocation entries.
It is a generic data path with many callers, not a demonstrated enemy-grab
method. Addresses deeper in the raw stack scan are not an unwind. The log
sequence is grab busy, throw gesture / second LB pulse, native grab empty,
then the fault. Throw token 4 was armed for entity `1B83BDB0`; there is no
`NativeThrow APPLIED` for token 4. No matching Windows Application Error / WER
dump was found during the recorded interval.

**Uncertainty.** The origin and lifetime of the invalid source buffer are not
known. The same memcpy RVA appeared in an older, different FOV experiment, so
the RVA alone cannot identify the grab or throw hook as the cause. The
`bf3a860` fallback suppresses mod aim/velocity overrides for native
character/weapon holds, but has not been tested in the game and is not a
proven root-cause correction. It is not a reason to repeat the crashing trial.
Current-EXE disassembly of `00878930` confirms three separate smart-reference
fields at fire receiver `+580`, `+584`, `+590`. The mod's `00879C88` compare
hook sits in a branch that uses `+584` and may also use `+590`; neighboring
compares at `008797FC` and `00879A77` serve other branches. The hook address
alone does not prove an ordinary-prop-only path, so the older source comment
is too strong. The last crash log has no `NativeThrow APPLIED` for token 4;
therefore no throw-velocity write is demonstrated at the fault. The
`00E37C10` hash/relocation routine has many unrelated callers. Raw-stack
`00A1DF9B` is a return after an animation-vector insert but is not in the
validated EBP chain and cannot be used to blame animation data.
Current-EXE disassembly of the native aim consumer `00C62F50` shows its
camera-matrix getter call at `00C6300F`, followed immediately by `MOVAPS`
copies of the forward and origin rows at `00C63011..00C63015` into its local
query setup. This rejects the narrow hypothesis that this call site retains
the mod's temporary matrix pointer for a later asynchronous read. It does not
explain the invalid buffer in the unrelated hash/relocation call chain.
Next evidence must connect the invalid buffer's producer and lifetime to a
specific call/interaction branch before choosing a correction. The existing
VEH record is first-chance; the user's report establishes that the play session
ended, but does not prove this exception was unhandled.

## Tentacle/world transforms

The latest user test accepted the visual separation: tentacles follow head
rotation while enemies, vehicles, and world remain stationary. The mesh-scoped
draw rule in `src/stereo_replay.inl` uses verified VB/IB, stride and stream
offset evidence rather than shader hash alone. No transform or stereo code is
changed on this branch. The existing matched capture predates this accepted
trial, so it is supporting draw-identity evidence, not a new headset PASS.

## Performance (separate workstream)

The latest trial measured D3D9 live plus replay near 8–10 ms in several
segments; it excludes D3D11/OpenXR work and does not establish a stable 90 Hz
frame. Earlier heavy gameplay segments were slower. No performance change is
made until independent frame pacing and the full CPU/GPU split identify the
dominant cost in a matched scene.
