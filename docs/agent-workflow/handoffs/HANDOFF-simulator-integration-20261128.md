# Handoff: Simulator integration for ladder, fog, and grab

**Date**: 2026-11-28
**Task ID**: simulator-integration-20261128
**Role**: MAIN
**Branch**: codex/darkness2-simulator-integration-20261128
**Worktree**: C:/Users/Trilex/Darkness2VR-simulator-integration-20261128
**Candidate base**: 1f8f198b with the ladder dismount applied at c98da8d5
**Integration commit**: pending acceptance

## Active grab

The existing protected camera getter served the inventory ray. The prop
grab request reached native acquisition through an independent call chain.
The stock-aligned control reached the acceptance function at 00909310 from
00CE9CC5 with a 00FE1CF8 cylinder and then attached it. The off-axis control
under the same observer did not reach acceptance. This was a selection
failure upstream of the threshold at 009096E0.

Hardware observation of the selected actor's fire object identified the live
query entry at CE9A5C and its parent C0BEE6. The stock code also calls this
entry from CC86DA. The two paths prepare transforms through 64FD90 and BF2750
before the native selection routine CB0040. The protected bridge now supplies
the tracked ray for those exact native call chains. The existing body
camera, visibility tests, native candidate references, range and grab
physics remain under the game. Throw selection still uses its own scope.
The independently executed x86 prologues established the CB0040 frame and
the caller return at frame +48. Stack-proof SHA-256
`91E838CEBED60EE731FED3B3E5D1D2296E446FB32EDC148319AB9464685ACEDF`.

## Evidence

- Diagnostic build SHA-256
  `ED1680758B6CC2D74F49B09C5C60955D1518D70815BE0626E3CB848832712BBB`.
  x86 ABI fixture passed 151,040 checks, including the active and unrelated
  caller frames. All 11 protected-profile and 50 capture validator tests passed.
- Private run `chapter-private-map-grab-retest`, PID 22156, loaded the copied
  crane checkpoint without using the Steam save. On the left cylinder trial
  Jackie's stock position and forward stayed unchanged, while the tracked
  direction was (-0.28030, 0.54145, 0.79263), 106.3 degrees from stock forward.
  Native acquisition reached 00909310 with candidate `3C98D770/00FE1CF8`.
  A separate read-only owner trace observed that exact reference in +558
  and then +580 about 102 ms later.
- `v2-cylinder-native-attachment.json` SHA-256
  `151997E75652052824F3D875195DF56CDB2069FE1B2FDBFD8858F839FAEBB791`.
  `v2-cylinder-crane-native-acquisition.log` SHA-256
  `1A9895AEDA0989CFA259FFE960E04FAC6711520CF1975FB9C087C7715EDD0B2C`.
  `v2-cylinder-settled.jpg` SHA-256
  `81457E44C0B15D5E0C217E11DD1BD3171925DEEFB1BA7B7F4D8D463674BC6A35`.
  The native observer used the inactive slot on the selected game thread
  and detached with zero register restoration failures.
- Ladder and Villa height fog passed separately. See the ladder-specific and
  fog-specific handoffs for those captures. Independent Villa shadow mask
  verification and physical headset acceptance remain open.

## Throw selection and stroke fallback

The protected release now retains two directions from the same valid request.
Gaze or controller aim feeds native target selection, and the measured hand
stroke feeds the free throw. A successful native target trajectory keeps
its calculated velocity and spin. The existing optional solver-failure
elevation setting remains available for that specific failure path.

The private `chapter-private-throw-routing` run used candidate SHA-256
`6A469FAA13F65EDD59DFC1EFB6302389C1D67CB3B39EE232FEECE2AD98C4F864`.
The simulator independently aimed and moved the left controller with a
49.44-degree separation. The first cylinder had no admitted target.
The log reported `route=stroke`, native speed 100.000, and velocity
(84.393, 30.492, -44.137), matching the measured stroke in the native
world basis. The stereo capture shows the impact flash near the crane cab.
The grapple released its held reference. The subsequent encounter killed
the private avatar before the next cylinder could be used on a hostile.
The live hostile trajectory result is still pending.

- `free-stroke-held-stroke.json` SHA-256
  `29A26DD3D78610EF998C8EEB2AA18929B5A07F54E059649EB02982BC1A5E43A7`.
- `free-stroke-impact.jpg` SHA-256
  `31759F28282C3358E5BE1ADD8ECE3B691D3FF85A0A6A243D68525A7AE8F076E8`.
- Protected release fixture passed 1,242 checks. This includes closest-angle
  ordering, target solver preservation, no-target stroke, absent selector
  stroke, speed and W preservation, and existing owner/input gates.

Combined grab/fog/ladder build `ED168075` loaded the copied warehouse alley
and reached the chain-link barrier at (116.957, -3.250, -20.500). That route
did not reach the ladder. The separate proven ladder run still applies to
the exact same ladder implementation.
