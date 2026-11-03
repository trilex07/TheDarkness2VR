# Correct native cinematic control contract (2026-11-03)

Pinned Steam EXE SHA256: D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1.

The previous reader used the wrong object tables and misidentified adjacent
boolean fields. This is a functional correction, not a new timing heuristic.

- RegionMgr FC7D00+1AC / 6CF490 returns the playing reference at manager+268.
- Observed restaurant reference points to native Cinematic table FCA7E0.
  Native registration EAC3D0 uses metadata10ED3B8 and literalF5BD18=Cinematic.
- FCA7E0+30 points to serializer47FC00. The complete three-field serializer
  range480140..4801A3 has SHA256
  6bc9c063780409106d45797853b8c49f0d3333b75178f2c758777b610a82437c.
- 480148 descriptor10A9174, address operand480156: object+7BC is
  mCanDamagePlayer, NOT mFreeCam. Descriptor string pointerF523FC; description
  says whether non-free-camera cinematics permit damage to the player.
- 480169 descriptor10A9168, address operand480177: object+7BD is mFreeCam.
  Descriptor string pointerF4FBEC; description says not to take camera control.
- 48018A descriptor10A915C, address operand480198: object+7BE is mOnRails.
  Descriptor string pointerF28F34; description explicitly allows limited look
  and firing while the cinematic controls the camera.
- All three descriptor names/descriptions independently read in own simulator
  process38076, 2026-11-03. No writes to native objects, EXE or IAT.
- FCA7E0+238 points to B7E570, native IsPlaying checks object+708==2.
  Complete13-byte getter SHA256:
  44efa516ae91b16dc7382ca5cad24c8864750aa44f05a788b83a8f79dd78cc48.

Passive theater requires this exact supported type, state2, FreeCam=false and
OnRails=false. Free-camera NPC animation and interactive rail segments remain
in gameplay presentation. Unknown object types remain unknown. Existing manual
60-second override remains available. No claim that native firing for every
special avatar has been verified follows from this classification.

Regression fixture exercises the actual reader: damage permission cannot turn
cinema off, the correct FreeCam byte does, OnRails stays interactive, a retained
nonplaying object cannot enable cinema, malformed bools/foreign tables reject.
Actual runtime transitions after this correction are recorded below.

The inherited entity method598C00 and its+2CC condition are not evidence of
camera takeover; their use for classification was investigated and rejected.

## Simulator runtime witness

Own process46660, diagnostic DLL6BE0361A503FE2617A7E0D5C5B4404F2E23BFB3BDD1B4681BFFF6BE8DDB0F90E:
native contract admitted. Villa Continue, elevator/NPC view and walking stayed
in gameplay; two controller poses visibly moved the right hand/sleeve.
Fresh New Game restaurant: object2A818DA0, exactFCA7E0, running1,
FreeCam0/OnRails0 selected Scene3D at frame71398. It returned to gameplay at
76055; another passive clip selected Scene3D at76078, returned at78594.
Look tutorial advanced with two native left-stick-look steps. Passive clip
at94560 ended at95169, then object0F27F5C0 ran with OnRails1 and takesCamera0.
The restaurant walk remained full gameplay with both hands visible. Later
object2B2295C0 also had OnRails1. Presented video at101252 selected Video2D;
passive clip at102074 selected Scene3D, then OnRails1 object37D4DC80 returned
to gameplay at103391. These are actual native flags and runtime decisions,
not synthetic state fixtures. The simulator screenshot of a passive fade was
mostly black and does not independently establish theater screen dimensions
or physical HMD comfort. Private log/screenshots retained in the task lab.

No claim of native QTE, execution, story, Darkling or carnival scene completion
follows from this restaurant run. No game EXE/native object patch was used.
