"""Original model-specific weapon silhouettes for the VR weapon wheel.

Each model is authored as outer contours (any simple polygon) plus cut contours
(holes: trigger guards, lever loops, sight windows, stock frames, slots). The
coordinates are hand-authored from proportions of the in-game models and their
real-world counterparts; nothing is traced from screenshots or extracted from
game data. decompose() turns the filled region into convex pieces for the
triangle-fan runtime: exact scanline trapezoids, then greedy vertical merging
into larger convex polygons (<= MAX_POINTS vertices each).

Canvas 0..100, y down, muzzle to the right. Keep ink inside x 3..97, y 18..92 so
an icon placed on any wheel slot stays inside the wheel disc.
"""

MAX_POINTS = 8
EPS = 1e-6


def box(x0, y0, x1, y1):
    return [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]


def inside(x, y, poly):
    hit = False
    for i, a in enumerate(poly):
        b = poly[i - 1]
        if (a[1] > y) != (b[1] > y) and x < (b[0] - a[0]) * (y - a[1]) / (b[1] - a[1]) + a[0]:
            hit = not hit
    return hit


def _clean(chain):
    """Drop repeated and collinear interior points of a top->bottom chain."""
    out = []
    for p in chain:
        if out and abs(p[0] - out[-1][0]) + abs(p[1] - out[-1][1]) < EPS:
            continue
        out.append(p)
    changed = True
    while changed and len(out) > 2:
        changed = False
        for i in range(1, len(out) - 1):
            a, b, c = out[i - 1], out[i], out[i + 1]
            if abs((b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])) < 1e-7:
                out.pop(i)
                changed = True
                break
    return out


def _polygon(piece):
    left, right = piece
    poly = list(right) + list(reversed(left))
    clean = []
    for p in poly:
        if clean and abs(p[0] - clean[-1][0]) + abs(p[1] - clean[-1][1]) < EPS:
            continue
        clean.append(p)
    if len(clean) > 1 and abs(clean[0][0] - clean[-1][0]) + abs(clean[0][1] - clean[-1][1]) < EPS:
        clean.pop()
    # remove collinear points around the closed loop
    changed = True
    while changed and len(clean) > 3:
        changed = False
        for i in range(len(clean)):
            a, b, c = clean[i - 2], clean[i - 1], clean[i]
            if abs((b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])) < 0.06:
                clean.pop(i - 1)
                changed = True
                break
    return clean


def convex(poly):
    sign = 0
    for i in range(len(poly)):
        a, b, c = poly[i - 2], poly[i - 1], poly[i]
        t = (b[0] - a[0]) * (c[1] - b[1]) - (b[1] - a[1]) * (c[0] - b[0])
        if abs(t) < 1e-9:
            continue
        s = 1 if t > 0 else -1
        if sign and s != sign:
            return False
        sign = s
    return True


def decompose(outers, cuts):
    contours = outers + cuts
    levels = sorted(set(round(y, 6) for p in contours for _, y in p))
    pieces = []
    for lo, hi in zip(levels, levels[1:]):
        mid = (lo + hi) * .5
        edges = []
        for p in contours:
            for i, a in enumerate(p):
                b = p[i - 1]
                if min(a[1], b[1]) < mid < max(a[1], b[1]):
                    slope = (b[0] - a[0]) / (b[1] - a[1])
                    bias = a[0] - slope * a[1]
                    edges.append((slope * mid + bias, slope, bias))
        edges.sort()
        runs = []
        for a, b in zip(edges, edges[1:]):
            if b[0] - a[0] < 1e-7:
                continue
            x = (a[0] + b[0]) * .5
            fill = any(inside(x, mid, p) for p in outers) and not any(inside(x, mid, p) for p in cuts)
            if fill:
                if runs and abs(runs[-1][1][0] - a[0]) < 1e-7:
                    runs[-1] = (runs[-1][0], b)
                else:
                    runs.append((a, b))
        for a, b in runs:
            left = [(round(a[1] * lo + a[2], 4), lo), (round(a[1] * hi + a[2], 4), hi)]
            right = [(round(b[1] * lo + b[2], 4), lo), (round(b[1] * hi + b[2], 4), hi)]
            pieces.append((left, right))
    # Greedy vertical merge: a piece directly above another with a shared edge.
    changed = True
    while changed:
        changed = False
        for i, a in enumerate(pieces):
            for j, b in enumerate(pieces):
                if i == j:
                    continue
                if abs(a[0][-1][1] - b[0][0][1]) > EPS:
                    continue
                if abs(a[0][-1][0] - b[0][0][0]) > 1e-4 or abs(a[1][-1][0] - b[1][0][0]) > 1e-4:
                    continue
                left = _clean(a[0] + b[0][1:])
                right = _clean(a[1] + b[1][1:])
                poly = _polygon((left, right))
                if len(poly) > MAX_POINTS or not convex(poly):
                    continue
                pieces[i] = (left, right)
                pieces.pop(j)
                changed = True
                break
            if changed:
                break
    return [_polygon(p) for p in pieces if len(_polygon(p)) >= 3]


# ---------------------------------------------------------------- pistols
SPECS = {
 'EmptyHand': ([
  [(32,42),(32,24),(33,19),(36,17),(39,19),(40,23),(41,42),(43,43),(44,14),(45,10),(48,9),(51,11),(52,15),
   (52,42),(55,43),(56,19),(58,15),(61,15),(64,18),(64,23),(63,46),(66,47),(68,30),(70,27),(73,28),(75,31),
   (74,55),(71,67),(65,76),(61,83),(61,92),(38,92),(38,81),(30,70),(17,52),(15,46),(16,42),(19,41),(23,43),(32,53)],
 ], []),

 # Colt MK IV Series 70: long flat slide, guide rod, beavertail-less spur
 # hammer, swept-back walnut grip, rounded trigger guard.
 'M1911': ([
  [(13,29),(84,29),(86,30),(86,40),(13,40)],              # slide
  [(84,32),(89,32),(89,39),(84,39)],                       # bushing + guide rod
  [(16,26),(22,26),(22,29),(16,29)],                       # rear sight
  [(79,26.5),(83,26.5),(83,29),(79,29)],                   # front sight
  [(8,30),(12,28),(15,30),(15,35),(11,36)],                # spur hammer
  [(18,40),(58,40),(58,44),(18,44)],                       # frame / dust cover
  [(14,40),(36,40),(37,45),(30,77),(28,80),(12,80),(9,77)],# grip, swept back
  [(35,43),(52,43),(56,46),(55,53),(51,58),(36,58)],       # trigger guard outer
 ], [
  [(38,46),(51,46),(52,49),(50,54),(38,55)],               # guard opening
  box(19,31,20.4,38), box(22,31,23.4,38), box(25,31,26.4,38),  # slide serrations
  [(19,53),(24,53),(25,57),(23,60),(18,60),(17,57)],       # grip medallion
  box(15.5,67,26,68.4), box(14.6,71,25.2,72.4),            # lower grip checkering
 ]),

 # Colt King Cobra: tall cylinder, full-length underlug, vent rib, spur
 # hammer, rounded combat grip.
 'Cobra': ([
  [(50,29),(92,29),(92,37),(50,37)],                       # barrel + rib
  [(52,37),(92,37),(92,43),(60,43),(53,40)],               # full underlug
  [(86,25),(91,25),(92,29),(84,29)],                       # ramp front sight
  [(27,28),(52,28),(52,33),(27,33)],                       # top strap
  [(29,31),(51,31),(54,34),(54,48),(51,51),(29,51),(27,48),(27,34)],  # cylinder
  [(19,29),(29,29),(29,52),(21,53),(17,45)],               # frame rear
  [(13,25),(18,24),(22,29),(21,33),(16,32)],               # hammer spur
  [(19,48),(30,50),(28,62),(24,76),(19,81),(12,81),(8,77),(12,62)],  # grip
  [(29,49),(46,50),(49,55),(46,63),(34,64),(30,60)],       # trigger guard
 ], [
  box(31,35.5,50,37.4), box(31,43.5,50,45.4),              # cylinder flutes
  box(58,30.6,64,32), box(68,30.6,74,32), box(78,30.6,84,32),  # vent rib
  [(33,52),(44,53),(44,59),(35,60)],                       # guard opening
  [(54,38),(56,38),(56,48),(54,48)],                       # cylinder / forcing-cone gap
  box(26.6,34,28,50),                                      # cylinder rear gap
 ]),

 # Desert Eagle Mk XIX: tall squared slide, sloped muzzle, ribbed top rail,
 # near-vertical blocky grip, square-front trigger guard.
 'DesertEagle': ([
  [(12,27),(84,27),(91,34),(91,46),(12,46)],               # slide / barrel block
  [(9,29),(13,28),(14,35),(10,35)],                        # round hammer
  [(18,46),(66,46),(66,50),(18,50)],                       # frame
  [(14,46),(36,46),(36,84),(13,84),(11,80)],               # straight grip
  [(35,48),(62,48),(62,52),(58,52),(56,63),(36,63)],       # square guard
 ], [
  box(24,27,27,29), box(33,27,36,29), box(42,27,45,29), box(51,27,54,29), box(60,27,63,29), box(69,27,72,29),
  box(17,32,18.5,42), box(20.5,32,22,42), box(24,32,25.5,42),  # rear serrations
  box(56,35,82,37),                                        # barrel/slide split line
  [(39,51),(55,51),(53,60),(39,60)],                       # guard opening
  box(16,58,32,59.6), box(16,64,32,65.6), box(16,70,32,71.6),  # grip grooves
 ]),

# ---------------------------------------------------------------- SMGs
 # IMI Micro Uzi: short slab receiver, mag through grip (32 rd, long), wire
 # stock folded over the top, barrel nut, front sight ears.
 'MicroUzi': ([
  [(22,33),(76,33),(78,35),(78,49),(22,49)],               # receiver
  [(78,37),(84,37),(84,45),(78,45)],                       # barrel nut
  [(84,39),(90,39),(90,43),(84,43)],                       # barrel
  [(70,27),(76,27),(76,33),(70,33)],                       # front sight ears
  [(26,28),(32,28),(32,33),(26,33)],                       # rear sight
  [(14,30),(76,30),(76,31.6),(14,31.6)],                   # folded stock arm
  [(14,30),(20,30),(21,50),(15,52),(13,50)],               # butt plate
  [(38,49),(52,49),(51,64),(39,64)],                       # grip
  [(40,63),(50,63),(52,91),(42,91)],                       # 32-rd magazine
  [(51,49),(66,49),(66,53),(62,61),(51,61)],               # trigger guard
 ], [
  [(53,52),(62,52),(60,58),(53,58)],                       # guard opening
  box(30,37,70,38.5),                                      # cocking slot
  box(42,52,48,61),                                        # grip panel
 ]),

 # H&K UMP45: sloped polymer receiver, straight .45 mag, closed grip/guard
 # loop, skeleton folding stock extended, top rail.
 'Ump45': ([
  [(27,33),(76,33),(82,37),(82,48),(27,48)],               # receiver
  [(82,39),(92,39),(92,44),(82,44)],                       # barrel
  [(34,30),(70,30),(70,33),(34,33)],                       # top rail
  [(74,26),(78,26),(80,33),(74,33)],                       # front sight hood
  [(5,32),(27,35),(27,47),(8,53),(4,51)],                  # skeleton stock
  [(32,48),(58,48),(58,53),(50,70),(38,74),(29,73),(29,68),(35,60)],  # grip loop
  [(60,48),(70,48),(70,78),(59,78)],                       # straight mag
 ], [
  [(9,37),(24,38),(24,44),(10,48)],                        # stock opening
  [(40,51),(54,51),(48,64),(40,66),(37,63)],               # grip/guard hole
  box(37,31,39,32.4), box(43,31,45,32.4), box(49,31,51,32.4), box(55,31,57,32.4), box(61,31,63,32.4),
  box(62,52,67,74),                                        # mag window
 ]),

 # H&K MP7A1: short PDW, mag through grip, folding vertical foregrip,
 # EOTech-style sight, collapsed stock.
 'Mp7': ([
  [(24,39),(74,39),(80,42),(80,50),(24,50)],               # receiver
  [(80,43),(88,43),(88,48),(80,48)],                       # barrel
  [(28,36),(76,36),(76,39),(28,39)],                       # top rail
  [(40,26),(54,26),(55,35),(39,35)],                       # red-dot sight body
  [(14,37),(24,37),(24,53),(17,56),(14,54)],               # collapsed stock plate
  [(32,51),(44,51),(42,70),(31,70)],                       # grip
  [(32,69),(42,69),(43,90),(33,90)],                       # magazine
  [(43,51),(56,51),(56,55),(53,61),(43,61)],               # trigger guard
  [(64,51),(71,51),(70,72),(65,72)],                       # vertical foregrip
 ], [
  [(42,28.5),(52,28.5),(52,32.5),(42,32.5)],               # sight window
  [(45,53),(53,53),(51,58),(45,58)],                       # guard opening
  box(17,40,21,50),                                        # stock slot
 ]),

# ---------------------------------------------------------------- shotguns
 # Winchester 1300 Defender: full butt stock, receiver, long barrel over
 # tube magazine, ribbed pump forend.
 'Defender': ([
  [(3,41),(22,40),(33,41),(33,51),(24,54),(4,63),(3,61)],  # stock
  [(33,38),(52,38),(52,52),(33,52)],                       # receiver
  [(52,38),(97,38),(97,42),(52,42)],                       # barrel
  [(52,43),(91,43),(91,47),(52,47)],                       # mag tube
  [(58,41),(80,41),(81,51),(57,51)],                       # pump
  [(94,35),(96,35),(96,38),(94,38)],                       # bead
  [(34,51),(47,51),(46,57),(41,60),(35,60)],               # guard
 ], [
  box(61,48,78,49.2), box(61,44.2,78,45.4),                # pump grooves
  [(37,53),(44,53),(43,56),(37,57)],                       # guard opening
  box(37,42,48,45),                                        # ejection port (mirrored)
 ]),

 # Remington 870 Masterkey ("Sawn off"): pistol grip, stub stock, short
 # barrel and tube, shell carrier on receiver.
 'SawnOff': ([
  [(16,37),(30,37),(30,45),(16,45)],                       # stub buffer
  [(30,33),(56,33),(56,48),(30,48)],                       # receiver
  [(56,34),(88,34),(88,39),(56,39)],                       # barrel
  [(56,40),(82,40),(82,45),(56,45)],                       # mag tube
  [(60,38),(78,38),(79,50),(59,50)],                       # pump
  [(29,47),(39,47),(34,72),(29,77),(21,77),(22,70)],       # pistol grip
  [(38,47),(52,47),(51,53),(46,57),(38,57)],               # guard
  [(32,26),(52,26),(52,33),(32,33)],                       # shell carrier
 ], [
  box(34,27.5,37,32), box(39,27.5,42,32), box(44,27.5,47,32), box(49,27.5,51,32),  # shells
  [(40,49),(48,49),(46,54),(40,55)],                       # guard opening
  box(63,46,76,47.2), box(63,42,76,43.2),                  # pump grooves
 ]),

 # Winchester 1887 lever shotgun, sawn-off: tall receiver, hammer spur,
 # big lever loop, barrel over mag tube, short curved grip-stock.
 'M1887': ([
  [(28,32),(48,32),(54,36),(54,49),(28,50)],               # receiver
  [(54,33),(95,33),(95,38.5),(54,38.5)],                   # barrel
  [(54,39.5),(86,39.5),(86,44.5),(54,44.5)],               # mag tube
  [(90,30),(92,30),(92,33),(90,33)],                       # bead
  [(23,26),(28,24),(32,30),(31,34),(26,33)],               # hammer spur
  [(5,41),(16,37),(28,34),(28,46),(18,51),(9,60),(5,59),(4,50)],  # sawn grip-stock
  [(27,48),(52,48),(52,54),(45,63),(31,65),(24,59)],       # lever loop
 ], [
  [(31,51),(46,51),(43,58),(33,60),(29,57)],               # lever opening
  [(34,36),(48,36),(48,44),(34,44)],                       # receiver side plate
  [(10,46),(20,42),(21,44),(11,49)],                       # stock/wrist separation line
 ]),

# ---------------------------------------------------------------- rifles
 # AKS-74U: short gas tube/handguard, muzzle booster, front sight block,
 # strongly curved magazine, triangular folding stock.
 'Kalashnikov': ([
  [(28,34),(62,34),(62,46),(28,46)],                       # receiver
  [(30,31),(60,31),(62,34),(28,34)],                       # top cover hump
  [(62,35),(78,35),(78,45),(62,45)],                       # handguard
  [(78,37),(82,37),(82,43),(78,43)],                       # barrel
  [(82,35.5),(93,36.5),(93,43.5),(82,44.5)],               # muzzle booster
  [(74,28),(79,28),(80,37),(73,37)],                       # front sight block
  [(4,34),(28,36),(28,44),(6,58),(3,56)],                  # triangle stock
  [(29,45),(38,45),(34,64),(25,63)],                       # pistol grip
  [(37,45),(48,45),(47,52),(44,55),(37,55)],               # trigger guard
  [(48,45),(58,45),(60,55),(50,58)],                       # curved mag 1
  [(50,57),(60,54),(64,65),(55,69)],                       # curved mag 2
  [(55,68),(64,64),(70,74),(62,79)],                       # curved mag 3
 ], [
  [(8,38),(24,39),(24,42),(9,50)],                         # stock opening
  [(39,47),(46,47),(44,52),(39,53)],                       # guard opening
  box(64,39,76,41),                                        # handguard slot
 ]),

 # Colt M4: collapsible stock, buffer tube, carry handle, triangle front
 # sight, round handguard, flash hider, slightly curved STANAG mag.
 'Carbine': ([
  [(3,36),(17,36),(19,38),(19,49),(15,53),(5,58),(3,56)],  # stock
  [(17,38),(30,38),(30,44),(17,44)],                       # buffer tube
  [(30,34),(56,34),(56,48),(30,48)],                       # receiver
  [(33,26),(52,26),(52,34),(33,34)],                       # carry handle
  [(56,35),(78,35),(78,47),(56,47)],                       # handguard
  [(78,39.5),(91,39.5),(91,42.5),(78,42.5)],               # barrel
  [(90,38),(96,38),(96,44),(90,44)],                       # flash hider
  [(73,35),(76,26),(78,26),(80,40),(73,40)],               # front sight post
  [(30,47),(37,47),(32,65),(24,64)],                       # pistol grip
  [(42,48),(51,48),(54,67),(45,68)],                       # magazine
  [(36,47),(44,47),(43,53),(36,53)],                       # trigger guard
 ], [
  [(36,29),(49,29),(49,32),(36,32)],                       # handle window
  [(5,41),(15,41),(15,47),(6,52)],                         # stock opening
  box(59,38,62,44), box(64.5,38,67.5,44), box(70,38,73,44),  # handguard vents
  [(38,49),(42,49),(41,52),(38,52)],                       # guard opening
 ]),

 # Kel-Tec RFB "Striker": bullpup slab, magazine behind the grip, forward
 # pistol grip, red-dot on full-length rail, short exposed barrel.
 'Striker': ([
  [(4,35),(74,35),(78,38),(78,50),(8,53),(4,51)],          # bullpup body
  [(78,40),(93,40),(93,44),(78,44)],                       # barrel
  [(91,38.5),(96,38.5),(96,45.5),(91,45.5)],               # flash hider
  [(20,32),(76,32),(76,35),(20,35)],                       # rail
  [(40,21),(56,21),(56,32),(40,32)],                       # red-dot sight
  [(26,50),(40,50),(40,72),(26,72)],                       # magazine (behind grip)
  [(55,50),(64,50),(59,73),(50,72)],                       # forward pistol grip
  [(63,50),(74,50),(73,55),(69,60),(63,60)],               # trigger guard
 ], [
  [(43,24),(53,24),(53,29),(43,29)],                       # sight window
  [(65,52),(71,52),(69,57),(65,57)],                       # guard opening
  box(8,39,18,47),                                         # butt cut-out
  box(28,54,38,56), box(28,60,38,62), box(28,66,38,68),    # mag ribs
 ]),
}

# Model display names (no runtime dependency, used by the review sheet).
DISPLAY = {
 'EmptyHand': 'Empty hand', 'M1911': 'M1911', 'Cobra': 'Cobra', 'DesertEagle': 'Desert Eagle',
 'MicroUzi': 'Micro Uzi', 'Ump45': 'UMP45', 'Mp7': 'MP7A1', 'Defender': 'Defender',
 'SawnOff': 'Sawn off', 'M1887': 'M1887', 'Kalashnikov': 'Kalashnikov', 'Carbine': 'Carbine',
 'Striker': 'Striker',
}

ICONS = {name: decompose(*spec) for name, spec in SPECS.items()}
