# Native inventory research for dual physical weapon wheels

Offline observations, 2026-10-31. These are evidence for further investigation,
not permission to write guessed object fields or call an unvalidated equip API.

Stock `DarknessII.exe` SHA256:
`D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1`.
Image base 0x400000; x86. Disassembled actual PE bytes using pefile/capstone.

## Verified getters

- Inventory vtable 0xFFCA10 +0x178 -> 0x8C62E0, hand -> weapon slot.
  Entry `8B4424048D04408B8481B0000000`; accesses the strong reference at
  inventory+0xB0+12*hand. If empty returns8; otherwise calls gun getter0x72BC70.
- Gun slot getter0x72BC70 bytes `8B89D40000008B4424048908C20400`:
  copies gun+0xD4 into caller-provided integer, returns that output, ret4.
- Vtable+0x184 ->0x5045A0 invokes hand-slot getter and returns runtime gun
  strong reference from inventory+0x20C+4*slot (8 is invalid/empty).
- Vtable+0x14C ->0x7185C0 returns saved weapon strong reference from
  inventory+0x12C+28*slot; slot>=8 returns empty sentinel0x10A1D60.
  Caller must reject negative slots itself.
- Existing owned gameplay samples identify hand0=right, hand1=left. XR hands
  use0=left,1=right: never pass XR index directly as native hand index.

## Pending native setter investigation

Vtable+0x17C at0xFFCB8C contains bytes `D07E7400` ->0x747ED0.
Entry bytes `83EC085355568BE98B4C241C8B45008B804C01000057`.
Function ends0x748044 `83C408C20C00`: thiscall, three stack arguments.

Argument decoding from pushes/prologue:
1. hand index: load0x747F6F `[esp+0x1C]`, compute3*index, destination
   inventory+0xB0+12*index.
2. weapon slot: entry0x747ED8 `[esp+0x1C]` before fourth saved register;
   invokes native saved-reference getter+0x14C.
3. boolean: byte `[esp+0x24]` after four saved registers/local8.

The setter clears matching weapon reference from up to4 hand states, balances
native strong references, replaces the selected hand's saved reference and
boolean, sets dirty notification. Thus direct assignment to inventory+0xB0
would omit ownership/dirty semantics and is unacceptable.

Direct caller0xC49F30 ends ret12 and forwards same arguments at0xC49F68:
`8B4424185053578BCEE85ADFAFFF`. It adds state restrictions using +0xC78
and stores previous primary slot at+0xC8C. Determine its owning vtable and
normal equip call chain before choosing a callable entry; calling the base
implementation could bypass game-specific restrictions. A raw PE pointer search
finds that wrapper only at0xFFD634, not at the observed inventory vtable's slot
0xFFCB8C. Do not substitute a method from another inventory class.

Native call sites0x44592D/0x445944 pass `(hand0/1, slot, false)` through virtual
+0x17C. Sites0x64EC9E/0x64ECAF pass `(0,slot,false)` and `(1,8,false)`.
Other call sites use true/dynamic booleans; the meaning is not established.
These call sites support the decoded argument order but do not prove legal
slot/rifle/off-hand combinations, ownership of every caller or simulation phase.

## Classification and UI constraints

Slot ids0..7, invalid8 are not firearm-model ids. Existing ObserveFirearm
classifies shared fire-mode implementation as Charged5 for observed different
guns; it must not choose model-specific icons. Need actual owned gun resource
identity. Do not show every icon as unlocked, clone one native gun into two
hands, bypass game equip rules, or claim per-hand selection already works.

Visual reference catalog: https://www.imfdb.org/wiki/The_Darkness_II contains
in-game screenshots separately from real firearm photographs. Revised artwork
must be newly authored and track the game silhouettes, not copied screenshot
pixels. See assets/weapon-wheel/icons/README.md for final visual sources.

## MAIN findings 2026-10-31 23:xx (objdump of the pinned EXE)

- Slot ids are runtime globals: two-handed `[0x1110258]` (.bss, 0 unless written),
  one-handed `[0x10CC248]`=1 and `[0x10CC24C]`=2 (.data). The runtime reads them
  every sample and refuses the loadout unless they are distinct and in 0..7.
- Native combos produced by the game itself through inventory vtable+0x17C:
  dual `(hand0=slot1, hand1=slot2)` at 0x445925 (only when slot2 saved ref is
  non-empty), two-handed `(hand0=slot0, hand1=8)` at 0x64EC96, draw/single
  `(hand0=slot1 or slot2, ...)` at 0x8BE7E0 (when hand0 is empty).
- `inventory+0x7B9` swaps which one-handed slot belongs to which hand
  (0x8BE7FB `80 BE B9 07 00 00 00`, 0x6FDC2C).
- HUD state at 0x6FDCE8: hand0 slot == two-handed -> 0, == slot1 -> 1, == slot2 -> 3.
- Saved-reference getter 0x7185C0 = `[inv+0x12C+28*slot]` refblock, `[refblock]`
  object; ownership test = object != 0 (same test the natives use).

Decision for the first wheel build: the wheels REQUEST stock pad actions
(DPAD_RIGHT two-handed, DPAD_LEFT one-handed/dual) and verify the result in the
native hand slots within 1.5 s. The +0x17C setter is NOT called: the protected
capture runs on the render thread and a gameplay mutation there is unproven.
Gun model identity: `[WeaponWheelProbe]` logs the strings each saved weapon
object points at (read-only, once per object); keyword matches pick the model
icon, otherwise the slot family icon is shown.
