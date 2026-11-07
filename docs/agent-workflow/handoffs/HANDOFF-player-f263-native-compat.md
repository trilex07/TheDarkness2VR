# F263 player build compatibility

Branch: fix/player-f263-native-compat. Base: 26c8dca8855e34a9dcb2d561db6e8887d568e89c (public 1.0.8 runtime source).

The supplied private EXE SHA256 is f263b81b950930f9244e8f5feae78de19689ed0c3de6e3a2923a85d6d4f9fdfa. This is a separate player from EF306DE6. No EXE bytes are shipped.

## Proven mechanism
Before this change, the exact identity gate rejects F263 before external Present observation starts. Installer consent and renderer discovery do not affect runtime admission. The EF306DE6 log demonstrates the gate failure; it is not a runtime trace for F263.

## Offline comparison with accepted D46BB6CB
- Whole .text (11,442,176 raw bytes), .data, .tls, .version and .reloc are byte-identical.
- PE machine 014C, timestamp 4F68A875, image base 00400000, image size 00DEC000 and entry RVA 00A1CDAB match.
- Import names/ordinals and IAT layout match. First 0x2DEC bytes of .rdata match.
- .rdata differs at 161,884 bytes and .rsrc at 16 bytes; no claim about purpose/edition/localization.
- All 71 source-literal read-only addresses in 0xEEB000..0x109A000 have identical 512-byte windows. This is supplementary, not an exhaustive indirect-reference proof.
- Existing runtime byte pins, function/table digests, owner checks and image-write guard remain enforced.

## Change (user-directed warning-only EXE SHA policy)
Whole-file EXE hashes are diagnostic only. An unknown or unreadable hash never vetoes native admission. KnownRuntimeSha supplies a log label, not permission. Present/source-size retain the PE layout and exact instruction/import checks. View, constant ownership, query preload and optional gaze observer retain their native byte/function/table/owner checks, but no whole-file SHA gate. Unknown hashes emit a warning before those validations. Codec DLL and signed-package integrity checks are outside this EXE policy and unchanged.

The independent EF306DE6 build is now allowed to attempt those structural checks, but its full native layout and actual VR remain unverified. This is not a blanket compatibility claim for other editions. No guessed address or generic unsupported hook was introduced.

## Verification
- x86 PE32 /O2 DiagnosticCapture build PASS (release symbols omitted: local MSPDB140 debug-server mismatch; /O2, /MD and capture architecture unchanged). DLL SHA256: A14CA6367330CC9705D18360F073383ACCDDEFA302510C5FD0243DCC781B2580.
- New native identity test PASS: unknown/null/empty hashes do not veto, incorrect PE fields still reject.
- 19 Python tests PASS: Present readiness, stereo transfer ordering, overlap contracts.
- x86 native Present patch ABI/rollback, image Present observer ABI/Reset/recreation/recursion, source quality fixtures PASS.
- No Steam writes or game launch. F263 OpenXR session, headset display and gameplay PENDING.

## Player acceptance
Install refreshed candidate with existing backup protection, launch VR normally, check menu and gameplay in both eyes with head tracking and one weapon. Send new d3d9.log after closing. Require successful ImagePresent/native forwarding, native-view admission and actual OpenXR submission receipts. Roll back through installer if it regresses. EF306DE6 may attempt native contract validation; do not claim its physical result from the F263 offline comparison.
