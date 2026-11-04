"""Synchronize DLC font definitions with the installed base game's font atlases.

The DLC has its own font TOCs but no English glyph textures. It must use the
same UV/metric/code tables as the repaired base atlases. No texture is changed.
"""
import argparse
import json
from pathlib import Path
from polish_pack import Cache, require, sha

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('cache','extract','output'):p.add_argument('--'+key,type=Path,required=True)
    a=p.parse_args();require(not a.output.exists(),'Fresh output required')
    before,after,changed={},{},[]
    for stem in ('H.Font','B.Font'):
        cache=Cache(a.cache,stem)
        replacements={path:(a.extract/stem/path).read_bytes() for path in cache.entries
                      if path.startswith('Temp/FontCache/') and (a.extract/stem/path).is_file()}
        require(len(replacements)==25,'Expected complete native font overlay')
        cache.write(a.output/'payload',replacements)
        for ext in ('.cache','.toc'):
            name=stem+ext;before[name]=sha(a.cache/name);after[name]=sha(a.output/'payload'/name)
        changed.extend(stem+'/'+path for path in replacements)
    (a.output/'manifest.json').write_text(json.dumps(dict(kind='font-definition-repair',
        before=before,files=after,resources=changed,textures='UNCHANGED',
        cache_relative='Cache.DLC/PCPatchManifests/Cache.Windows'),indent=2))
    print('BUILD_VERIFIED 25 synchronized native font pairs; no atlas/UV generation')

if __name__=='__main__':main()
