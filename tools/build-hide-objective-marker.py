"""Hide only destination arrows in native HUD; keep objective text and world FX.

Game-derived payload stays in the local package, never in Git. Replace a
comment with an equal-length visibility override, preserving serialization.
"""
import argparse
import hashlib
import json
from pathlib import Path

from polish_pack import Cache, require, sha

RESOURCE = 'D2/Menus/ObjectivePopUp.lua'
SOURCE_SHA = '5096a35e4809f6c255a72a301444ca79925e3176d6eab9255937663a1fd5e153'
PACKED_SHA = '30d88a89d0c5bfd99a9ce2c92a66d7bfe5daa2cae2b4293eed695cd89f1d5eab'
ANCHOR = b'\t-- Perform floor on x/y to eliminate decimal value so fractions of a screespace coord'
OVERRIDE = b'\tif arrowType == ARROWTYPE_Objective then vis=false;arrowVis=false end'


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    for name in ('cache', 'source', 'out'):
        ap.add_argument('--' + name, type=Path, required=True)
    a = ap.parse_args()
    require(not a.out.exists(), 'Fresh package required')
    original = a.source.read_bytes()
    require(hashlib.sha256(original).hexdigest() == SOURCE_SHA, 'Extracted preimage differs')
    require(original.count(ANCHOR) == 1 and len(OVERRIDE) <= len(ANCHOR), 'Ambiguous anchor')
    offset = original.index(ANCHOR)
    replacement = OVERRIDE.ljust(len(ANCHOR), b' ')
    patched = original[:offset] + replacement + original[offset+len(ANCHOR):]
    require(len(patched) == len(original) and patched[:offset] == original[:offset]
            and patched[offset+len(ANCHOR):] == original[offset+len(ANCHOR):], 'Unexpected mutation')
    cache = Cache(a.cache, 'H.Script')
    _, start, packed, size = cache.entries[RESOURCE]
    require(size == len(original), 'Resource size differs')
    with (a.cache / 'H.Script.cache').open('rb') as f:
        f.seek(start)
        require(hashlib.sha256(f.read(packed)).hexdigest() == PACKED_SHA, 'Installed preimage differs')
    cache.write(a.out / 'payload', {RESOURCE: patched})
    names = ('H.Script.cache', 'H.Script.toc')
    manifest = dict(schema=1, kind='hide-objective-marker',
                    before={n: sha(a.cache/n) for n in names},
                    files={n: sha(a.out/'payload'/n) for n in names},
                    source_sha256=SOURCE_SHA, resource=RESOURCE,
                    override=OVERRIDE.decode().strip(), payload_sha256=hashlib.sha256(patched).hexdigest(),
                    objective_text='unchanged', world_effects='unchanged',
                    headset='NOT RUN')
    (a.out/'manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
    print('BUILD_VERIFIED', a.out)


if __name__ == '__main__':
    main()
