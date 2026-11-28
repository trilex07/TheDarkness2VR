# Composite height fog from both eyes

MAIN, 2026-11-28. Branch `codex/darkness2-composite-fog-height-20261128`,
based on `2eb3b747`. Runtime candidate pending a free shared Steam lab.

The Villa elevator capture at frame 10015 supplied 127,307 same-surface
matches using the actual sampler 2 depth from each eye. The depth-producing
viewport used `[0.05, 1]`. The captured inverse at c10–13 already incorporates
that depth range and reconstructs height relative to the native camera. Applying
the viewport normalization again made the height reference error larger.

The measured native fog operation multiplies screen x and y by the sampled
depth before reconstruction. Holding the actual depth and captured bank fixed,
unscaled x/y reduced the Villa median height error from 0.0633446 to 0.00011054
game units. The p99 fell from 0.128150 to 0.00718115. Comparing crane frames
2990 and 4906 at 39,570 matching surfaces, the median difference between poses
fell from 0.0155766 to 0.000402775. These are reconstructed height measurements.

The original 489-word pixel shader has FNV-1a `33DE98C1` and SHA-256
`C519EDC89C34487FE036583F12BB07A144A525A6A5934290AE102CE1A14F3F79`.
The captured DOF-only version `A69BDC80` differs only in the blend at token 335.
The new variant changes the depth multiplier at token 212. A private c223.x
selector keeps the native multiplier for other draws. The selector is active
only with tracked stereo reconstruction, inverse c10 and the observed
fullscreen VS `24E3903D`. The saved sampler and native fog blend stay active.
Existing fog bypass takes precedence. If the driver rejects the new variant,
the creation path retries the existing DOF-only variant when requested.

Evidence is in `captures/simulator-20261128` in the simulator-fixes worktree.
`actual-fog-stereo-comparison.json` has SHA-256
`5A7E2C555A13289B8BE593F1F9EABA7561116C9306766DD60C7015DE6634FE1E`.
The Win32 diagnostic build, the actual-source selector and render binding
fixtures, 50 capture validator tests, and CPU Direct3D disassembly of all three
exact composite variants passed. The dedicated D3D9 test compiled. Candidate
DLL SHA-256 `5D962CEF2872EEC6F390A986FB90FB969A2EF299B00EE50247D0D354068C99B1`.

The accepted Steam installation is in use for the player's manual run. The
candidate has not been installed. In the next lab session capture the exact
Villa consumer and its fog multiplier in both eyes at the elevator and in the
lobby, then compare the reported highlight while moving the headset. The
separate Villa shadow measurement remains open.

At the elevator the retained shadow frame 10118 had three `C4D1FB10`
draws per eye. Ordinal zero gave exactly unity at all 22,227,532 valid
samples across both eyes. After moving into the lobby, frame 13298 had
just one draw per eye, so requesting ordinal one could not capture anything.
Request ordinals one and two from the elevator before moving. Details are
in `chapter-private-map-villa/villa-shadow-ordinal-analysis.json`.

## Villa runtime comparison

The private `chapter-private-map-villa-fog` run loaded `PokerHub.level` with
the exact candidate `5D962CEF`. Both eyes captured the original sampler 2
depth and the exact corrected composite bytecode, shader `FB479D02` with
SHA-256 `2DB8BE720362CA81D1890378AD2A442CA8A1C34C1932D223C114D91F6C43DC4A`.
The effect remained enabled. The actual depth captures and independently
disassembled shader support these reconstruction results:

- Elevator frame 6488, 128,534 matched samples. The median error against
  independently reconstructed height was 0.0638751 with scaled x/y
  and 0.00009874 with unscaled x/y.
- Foyer frame 12045, 129,699 matches. The corresponding values were
  0.0632917 and 0.00035235.
- Shifted-head corridor frame 15841, 129,490 matches. The values were
  0.0630266 and 0.00023832.

The candidate was viewed at the elevator, in the foyer and in the brighter
window corridor. The recorded device bank at each composite draw has
`c223=(1,0,0,0)` in both eyes. Physical headset acceptance is pending.
`villa-effective-shader-selectors.json` SHA-256
`F253E6A14AF5F3C5B577CA98EB120004249E158743EE4FB2540FEDBB43AB6D74`.

Shadow ordinals 1 and 2 at the elevator (frames 8804 and 9017) and
ordinal 0 in the foyer and window corridor (12139 and 15586) all produced
complete draw pairs. The shadow audit found no change in the selected
multiply-blended target. A separate count checked each of the four target
channels in each eye and also found zero changed valid samples. Choose a
different Villa light with a measurable mask before making any further
projection change for this issue.

All 28 observed shadow draws used the corrected selector, and the effective
inverse matched the independently derived geometric-head matrix within
`1.45e-6`. For the four measured draw pairs, matching actual depth between
eyes yielded 72,134 to 73,706 world points per frame. Corrected clip x/y cut
the median between-eye position difference from 0.00202–0.00374 to
0.00037–0.00073 game units. This measures shadow input geometry. The visible
mask test still needs a contributing Villa light.
`villa-shadow-position-audit.json` SHA-256
`C8D2B62CD113C252FF4CFE5AB9704F438829E7194F258DD25AB59770AA90ED94`.

Representative `villa-drawing-room-light.jpg` SHA-256
`5AD4CAE09DBDBB25A0A9AD50959E03C8CBF322042E1F0F741627CA85A93622F3`.
The private evidence includes the three height audits and the full capture
inventory `retained-captures.json`. The seven completed captures were moved
out of TEMP and all files rechecked by SHA-256. The private process closed
normally, verified the original Steam paths and released the lab.

## Contributing Villa shadow

Private run `chapter-private-map-villa-shadow`, PID 27204, used the same
`5D962CEF` build. Moving from the elevator onto the entrance hall floor
exposed a visible character shadow and a contributing `C4D1FB10` draw.
The exact shader and live `c223=(1,0,0,0)` were verified in both eyes.
The same-frame mask comparison held the native light constants equal and
matched actual depth at 161,282 points. Median absolute mask error was
0.000191 and p90 was 0.00378. After moving the head 0.12 m and changing its
yaw and pitch, a second frame matched 124,049 points with median error
0.000289 and p90 0.00544. The second shadow draw contributed a small
region and also passed with 329 points. Physical headset acceptance remains
pending.

- Entrance shot `villa-shadow-entry-new.jpg` SHA-256
  `DDF023759B4B9174E1EC768223726D1796A5E1951690BA61A565B48039A83A49`.
- `paired-pass-1622.json` and `paired-pass-1624.json` SHA-256
  `1C87F402D95903A21B1E5E365CC0F98A3FF9A5F2AB1D9DE0E064DA699DE1C1F4`
  and `6D002C3D5192C2F9D131B37FD085D3101B5DB8E5F5094072A7796C73611E8387`.
- Four full paired captures were moved out of TEMP after normal exit and
  hash-verified. `retained-captures.json` SHA-256
  `081C7AD44A338ADA65132C20F4C11AC71121C451A30C9DDACFB4D253FA43E538`.
