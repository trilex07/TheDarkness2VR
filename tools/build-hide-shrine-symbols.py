"""Build a private shrine-symbol repair; never modifies the installed game.

The user-confirmed symbols are TalentShrineBlackHoleEye's two color terms.
Keep its shader, alpha/depth behavior, mesh, shrine particles and interaction.
Inputs are owned-game data extracted separately and must not enter Git.
"""
import argparse
import hashlib
import json
from pathlib import Path
from polish_pack import Cache, require, sha

RESOURCE = 'D2/FX/Darkness/Relics/TalentShrineBlackHoleMat'
RAW_SHA = '3cba240e1c4efa883cc08b963944ab962dd6ad0bad90130f9f27779dda32512d'
PACKED_SHA = '6b6da76bc0ac52d217114ae50b08b855ae8eb9e94689127126c76874bf0ae1ec'
COLORS = (b'0.106157,0.008373,0.31118,1', b'0.254916,0.051122,0.701164,1')


def patch(data):
    require(hashlib.sha256(data).hexdigest() == RAW_SHA, 'Material preimage differs')
    result = data
    for name, color in zip((b'EyeColor', b'HiLiteColor'), COLORS):
        before = b'PS:' + name + b'={' + color + b'}'
        # Fixed byte width preserves the serialized property-string length.
        zero = bytes(48 if 49 <= c <= 57 else c for c in color)
        after = b'PS:' + name + b'={' + zero + b'}'
        require(result.count(before) == 1, 'Ambiguous color property')
        result = result.replace(before, after)
    require(len(result) == len(data), 'Serialized size changed')
    return result


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--cache', type=Path, required=True)
    ap.add_argument('--material', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    a = ap.parse_args()
    require(not a.output.exists(), 'Fresh output required')
    cache = Cache(a.cache, 'H.Misc')
    _, off, packed, size = cache.entries[RESOURCE]
    require(size == 1001 and packed == 616, 'Unexpected material entry')
    with (a.cache / 'H.Misc.cache').open('rb') as f:
        f.seek(off)
        require(hashlib.sha256(f.read(packed)).hexdigest() == PACKED_SHA,
                'Installed packed material differs')
    data = patch(a.material.read_bytes())
    cache.write(a.output / 'payload', {RESOURCE: data})
    names = ('H.Misc.cache', 'H.Misc.toc')
    manifest = dict(kind='hide-shrine-symbols', cache_relative='Cache.Windows',
                    resource=RESOURCE, before={n: sha(a.cache/n) for n in names},
                    files={n: sha(a.output/'payload'/n) for n in names})
    (a.output/'manifest.json').write_text(json.dumps(manifest, indent=2))
    print('VERIFIED one material, two color terms; shader and unrelated TOC entries preserved')


if __name__ == '__main__':
    main()
