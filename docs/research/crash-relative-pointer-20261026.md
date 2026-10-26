# E129EA: mod writes corrupt the input to native address derivation

MAIN base: `codex/perf-critical-path` / `e2cb9c8`.
Scope: the two recorded A/B failures, not a claim that every native crash is solved.
No Steam launch, deployment, new profiler or in-process experiment in this task.

## Exact offline reproduction

The model admits only installed EXE SHA-256
`d46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1`,
PE32/I386, base `00400000`, timestamp `4F68A875`, image size `00DEC000`.
It reads the original file and mutates only temporary Python bytearrays.
No game instructions, assets, EXE, private logs or disassembly exports are committed.

**B: six changed bytes are sufficient to produce D966AF14.**

- Native path `00705F20 -> 00897800 -> 00D174B0 -> 00E37C10 ->
  00D163B0 -> 004E0E40 -> 00E129EA`.
- Producer `00705F20` hashes 137 regions using relative starts at `0104A928`
  and lengths at `0104AB60`. The region addresses are relative to each entry,
  not relative to the table. The image-base word is `0104B660`.
- It then hashes caller return `008B7825` relative to the image base, as a
  little-endian DWORD. CRC starts at FFFFFFFF and is complemented on output;
  reflected CRC32 modeled by incremental `zlib.crc32` reproduces the results.
- `00897800` transforms that CRC with `00632EC0`, `006F4C30`, and XOR 07070707.
  At `00897837..0089786B`, the result XORs the encoded word and is added to
  that word's address. Word `0104B674` contains F858B6DA. The result is passed
  as the source address, not checked as a tracked-hand object.
- Original CRC **00E0B6C0**, key **07281356**, source **00755C00**.
- B's DLL base was **55F00000**; its verified NativeAimMatrix RVA **1FAA0**.
  The mod replaces six bytes at **00C63009** with `mov eax,55F1FAA0; nop`.
  This intersects region66, **00C62C00..00C63700**.
- That single replacement produces CRC **64EAF96C**, key **20394E7A**, source
  **D966AF14**, exactly the B exception's unmapped source pointer.

**A: the four-byte class-vtable replacement is sufficient to produce A1561184.**

- Corresponding producer/decoder: `006C6820 -> 008CF2E0 -> 00CB9640 ->
  00E37C10`, then the same buffer/hash/memcpy path.
- 113 region starts at `01035EC8`, sizes at `010360A0`, image-base word
  `010369C0`, encoded source word `010369F4`, caller return **00667CC4**.
  **006C6910 is an internal decoder return, not this caller input.**
- Key construction: rotate CRC right by one; shuffle using `00833F70`;
  XOR 03000300. The committed model specifies all masks and 32-bit wrapping.
- Original CRC **0058CDF0**, key **17145EE0**, source **00F36A00**.
- A's DLL base **55F00000**, NativeHandsPackHook RVA **20070**. Replacing class
  vtable slot **00FC2664** from **006C3890** to **55F20070** intersects region32,
  **00FC1E00..00FC2A00**.
- That single replacement produces CRC **7BED2D9C**, key **48B6F97C**, source
  **A1561184**, exactly the A exception's unmapped source pointer.

The two precise reproductions identify a **mod regression in hook installation**:
we changed native hash inputs used to derive valid addresses. A native heap-object
lifetime error is not needed to explain either recorded source value. Different
DLL function addresses explain why the resulting invalid pointers can differ.
First-chance VEH records still do not reveal final exception-handler disposition;
the observed process exits remain failed gameplay trials.

## Bounded source correction, and its functional limit

`BlockNativeImagePatches=1` in the diagnostic INI latches an OFF-by-default
guard before any installer. It rejects mod writes overlapping the main PE image,
including code, class vtables, and imports. It also refuses overflowing/unknown
intervals. Both proven mutation sites are covered at their actual write boundary;
the gameplay bundle rejects before installing its first hook. The guard also
covers other existing installers, EXE Present, resolution/input imports and the
generic COM/state-block table writers if their target ever aliases the EXE.
External D3D9 COM tables remain eligible. Native hash/decoder/validation code is
untouched; no exception swallowing, transient byte restoration during hashing,
unknown-instruction skipping or checksum substitute is introduced.

This is an **IMPLEMENTED / OFFLINE VERIFIED isolation candidate**, not a complete
VR crash fix. It explicitly requires `DisableOpenXR=1`, `StereoMode=0`, and
`MatchedFrameCapture=0`; incompatible configuration refuses DLL initialization
before installing hooks, with VEH/log cleanup. The optional pipeline-trace writer
is suppressed before it can start. The settings are not silently rewritten.
The EXE Present observer cannot run in this mode, so neither same-frame stereo
nor tracked-native input/hand/FOV hooks nor the IAT sizing path are available.
In particular, EndScene is not substituted for a verified Present boundary.
Existing renderer algorithms, image-quality parameters, source IK, tentacle and
throw logic, and persistent logging are unchanged. Default-OFF retains the old
behavior, including its known unsafe native-patch combination.

The remaining full-VR blocker is **image-preserving interception with proven
ownership/lifetime and a real presentation boundary**. A detour at getter
00504810, packer006C3890, camera class slotFFC85C or dispatcher95AC80 is not an
admitted replacement: those locations overlap other static region-table
candidates. Those overlaps do not establish which future native path executes;
they specifically defeat a claim that merely moving the detour is safe.
No guessed heap object, shortened vtable clone or new EXE patch was installed.

No additional dump is needed to reproduce these two source values. A later
different crash may require a terminal dump; it must not be conflated with this
proven pair or used to justify another general profiling session.

## Offline checks and preserved installation

- `python tools/native-camera/test_crash_relative_pointer.py --exe <installed-exe>`:
  **7/7 PASS**, including both exact faults, original valid sources, original-byte
  restoration, transforms/wrapping and rejection of an unknown image.
- `tools/frame-boundary-simulator/build-native-image-guard.ps1`: **PASS**, x86
  image-edge/overflow tests, all current patch classes, mono admission, actual
  existing Present Patch transaction refusing writes, default-OFF install/rollback.
- Existing actual x86 Present ABI/HRESULT/recursion/rollback/witness test: **PASS**.
- Existing tracked-gameplay gate: **1329 checks PASS**. Frame-boundary suite:
  **2327551 assertions, zero failures**, synthetic50000/timing25000.
- Matched capture **31/31**, readiness **8/8**, camera audit **7/7 PASS**.
- Production and diagnostic x86 DLLs: **build PASS**, saved privately under
  `dist/crash-relative-pointer-20261026`; no installation action.
- Diagnostic SHA256 `FFE5C5DBC5E6DE79392FFA224B6D4254AB97D24AB60AE5270F4E769BCC9F208C`.
  Production SHA256 `035A78CD5BCE8E3EB1797B43DAAC588C2A916A8823DCB0D1E534C1EBBFEDFA02`.

Fresh read-only hashes match the preserved `perf-log-ab-20261026-105015` baseline:
installed DLL `0A32C5B18AFA00B578DC8F86D1B4A1DA164393FA7399D665E3AE740F27E13E82`;
main INI `9405BBAA0C6F3BFB12621CE5FC1B56C00518264191BA3BA2BDBA54497058C613`;
diagnostic INI `9DD7FC7B61BC8C8877F8AA9B0FC0F6FE81322EAB481075864E21D0EBD544C7BC`;
local INI `2320B450FAEF34A17DE4A7E0A3E55D2A14A8DE98772E13B6F8A497EAA5FDDA20`.
EXE retains the admitted SHA above. Backups/untracked work are preserved.

IN-GAME VERIFIED: **NOT RUN**. HMD ACCEPTED/FIXED: **NO**. Full VR stability:
**BLOCKED** as specified above. No merge/deployment/next game launch authorized.

SIDE remains separate: `codex/shader-loading-prewarm` / `0f9c5f5` demonstrates
why an unverified loading prewarm is unsafe: binding mutates material state, and
failed PS/VS creation clears native bytecode slots. See that branch's focused
`shader-loading-prewarm-20261026.md`; no unsafe cache or driver-thread call added.
