# Controller playtest corrections

Merge the overrides into the current working candidate, keeping its quality,
tracking and optimization settings. L3+R3 returns to the existing recenter
handler in both physical and VR gamepad paths; triangle hold no longer recenters.
SubmissionStatus=0 hides the CPU/CULL overlay, without turning off optimizations.

The associated DLL keeps the selected avatar's protected human-hand packer and
the independently equipped pistol active during native demon holds. Human hands
use grip space when free and the pistol's aim space when equipped. The original
wrist-to-weapon socket calibration remains responsible for the visible grip.
Native target ownership, physics and character action contracts are unchanged.

The player-tentacle-only head-transform cancellation is skipped during acquisition,
attachment, character/retained-target holds, unknown or stale native feedback.
This leaves both tentacle and target in native world space. It does not add
controller steering of the held entity. Free tentacles retain the prior effect.

Reload dwell is at HMD-relative upper-leg height (1.05 m below the head,
0.23 m laterally). Existing tracking, pause, trigger, dwell and rearm gates remain.
The native gamepad reload action is still shared between equipped weapons.

Sleeve removal is a separate, reversible original-asset correction implemented
by `tools/hands_only_asset.py`. The stock resource identifies separate hand and
sleeve index ranges across all five LODs. Only sleeve triangles are degenerated;
hand indices, vertices, skin weights, skeleton and textures remain original.
It applies to normal FPJackieArms, not special asylum/crucifixion avatars.
Install and rollback require the private prepared asset package and a closed
game; see the original-hands section in
`docs/research/protected-weapon-controllers-20261028.md`. DLL/INI overrides alone
do not remove sleeves. Installation has been verified by extracting the modified
resource back from the game cache; appearance in the headset remains untested.

Offline x86 checks: protected hands 58,828 including exact native skin arithmetic;
protected weapon/native sampler 74,795; tentacle draw scope 19,069; actual gesture
body 774. Production and diagnostic DLL builds pass. These checks do not establish
the visible grip, native held-target continuity, reload comfort or FPS in a headset.
