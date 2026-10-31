# Weapon wheel icons

Original, detailed pale silhouettes of the concrete Darkness II player weapons
(user art direction 2026-10-31: model-specific, bone-white #EEE8DC ink, not
colored miniatures). Muzzle points right. Coordinates are hand-authored from the
proportions of the in-game models and their real counterparts (roster:
imfdb.org/wiki/The_Darkness_II); nothing is traced from screenshots or taken
from game files.

## Icons (`d2vr_weapon_icons::Icon`, append-only order)

| # | Enum | Model | Family | TwoHanded() |
|---|---|---|---|---|
| 0 | EmptyHand | open palm / unequip | EmptyHand | no |
| 1 | Pistol | generic fallback, draws M1911 | Pistol | no |
| 2 | Revolver | generic fallback, draws Cobra | Revolver | no |
| 3 | Smg | generic fallback, draws Micro Uzi | Smg | no |
| 4 | Shotgun | generic fallback, draws Defender | Shotgun | yes |
| 5 | AssaultRifle | generic fallback, draws Carbine | AssaultRifle | yes |
| 6 | M1911 | Colt MK IV Series 70 | Pistol | no |
| 7 | Cobra | Colt King Cobra revolver | Revolver | no |
| 8 | DesertEagle | IMI Desert Eagle Mk XIX | Pistol | no |
| 9 | MicroUzi | IMI Micro Uzi | Smg | no |
| 10 | Ump45 | H&K UMP45 | Smg | no |
| 11 | Mp7 | H&K MP7A1 (red dot, foregrip) | Smg | no |
| 12 | Defender | Winchester 1300 Defender pump | Shotgun | yes |
| 13 | SawnOff | Remington 870 Masterkey ("Sawn off") | Shotgun | no |
| 14 | M1887 | Winchester 1887 lever, sawn stock | Shotgun | yes |
| 15 | Kalashnikov | AKS-74U | AssaultRifle | yes |
| 16 | Carbine | Colt M4 | AssaultRifle | yes |
| 17 | Striker | Kel-Tec RFB bullpup | AssaultRifle | yes |
| 18 | Count | sentinel | | |

`TwoHanded()` is a presentation hint (Sawn off is dual-wieldable in the game);
it is NOT evidence of native carry rules. Inventory-to-icon identification is
MAIN-owned and still unproven.

## Runtime API (`weapon_icons.h`, the only runtime dependency)

- `Get(Icon) -> Shape { polygons, count }`; `Polygon { points, count }`, every
  polygon convex, <= 8 points, `Point { x, y }` in 0..100, positive Y down.
  Triangle-fan from vertex 0. Every fan triangle has area >= 0.02 units^2.
- `Family(Icon)`, `TwoHanded(Icon)`, `Name(Icon)` (display string).
- Components touch/overlap: draw icon ink opaque (or rasterize the union first).
  With per-triangle anti-aliasing (SVG/MSAA edge AA) hairline seams can show
  between adjacent pieces; opaque non-AA ink or a cached texture avoids that.
- Budgets (validated by the generator): <= 64 polygons and <= 80 triangles per
  icon; ink inside x 3..97, y 18..92 (EmptyHand 9..92) so slot icons stay inside
  the wheel disc. Heaviest model: Ump45, 64 triangles.

## Sources and outputs

- `model_silhouettes.py`: per-model outer contours + cut-outs (guards, lever
  loop, sight windows, stock frames, slots), decomposed into exact scanline
  trapezoids and greedily merged into convex pieces.
- `generate_icons.py`: source of truth; validates, then writes `weapon_icons.h`,
  one SVG per enum entry, `preview.svg`/`preview.png` (all entries) and
  `revision-preview.png` (models at 256 px plus every entry at 48 px).
  Pillow is optional (PNG only). Run: `python generate_icons.py`.
