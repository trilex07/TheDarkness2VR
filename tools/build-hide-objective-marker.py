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
VARIANTS = {SOURCE_SHA: PACKED_SHA,
    # PC patch overlay wins over the base script; both need the same override.
    '2883a51cb0bca5cc9f5553814131b8b516cf2390ec4a61fc403dfcba897f28f8':
    'c1e003a2de12f816dc07bd643b49c387cdec0703b74188a33326ebf9163d90c3'}
ANCHOR = b'\t-- Perform floor on x/y to eliminate decimal value so fractions of a screespace coord'
OVERRIDE = b'\tif arrowType == ARROWTYPE_Objective then vis=false;arrowVis=false end'


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    for name in ('cache', 'source', 'out'):
        ap.add_argument('--' + name, type=Path, required=True)
    ap.add_argument('--cache-relative', choices=('Cache.Windows','Cache.DLC/PCPatchManifests/Cache.Windows'), default='Cache.Windows')
    a = ap.parse_args()
    require(not a.out.exists(), 'Fresh package required')
    original = a.source.read_bytes()
    source_sha = hashlib.sha256(original).hexdigest()
    require(source_sha in VARIANTS, 'Extracted preimage differs')
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
        require(hashlib.sha256(f.read(packed)).hexdigest() == VARIANTS[source_sha], 'Installed preimage differs')
    cache.write(a.out / 'payload', {RESOURCE: patched})
    names = ('H.Script.cache', 'H.Script.toc')
    manifest = dict(schema=1, kind='hide-objective-marker',
                    cache_relative=a.cache_relative,
                    before={n: sha(a.cache/n) for n in names},
                    files={n: sha(a.out/'payload'/n) for n in names},
                    source_sha256=source_sha, resource=RESOURCE,
                    override=OVERRIDE.decode().strip(), payload_sha256=hashlib.sha256(patched).hexdigest(),
                    objective_text='unchanged', world_effects='unchanged',
                    headset='NOT RUN')
    (a.out/'manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
    print('BUILD_VERIFIED', a.out)


if __name__ == '__main__':
    main()
