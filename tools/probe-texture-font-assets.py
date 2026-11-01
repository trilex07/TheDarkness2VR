"""Offline extraction probe for current private asset candidates."""
import sys
from pathlib import Path
import json
import struct

sys.path.insert(0, 'C:/Users/Trilex/Darkness2VR/tools')
import manage_comic_textures as m

out = Path('test-output/texture-font-repair')
out.mkdir(parents=True, exist_ok=True)
backup = Path('C:/Users/Trilex/AppData/Local/Darkness2VR/backups/polish-comic-main-20261101-accepted-camera')
package = Path('C:/Users/Trilex/AppData/Local/Darkness2VR/packages/polish-complete-release-20261101/payload')
manifest = json.loads(Path('C:/Users/Trilex/Darkness2VR/extracted_textures/package/manifest.json').read_text())
targets = {i['path'] for i in manifest['items'] if 'GrabbyRevised_d' in i['path'] or 'JackieFPArms' in i['path']}
for stem in ('H.Texture', 'B.Texture', 'F.Texture'):
    m.extract_selective(backup, stem, targets, out/'original'/stem)
print('Original texture extract', sorted(targets))
for stem in ('H.Font', 'B.Font'):
    c = m.Cache(package, stem)
    targets = {p for p in c.entries if '/Fonts/' in p or p.startswith('Temp/FontCache/')}
    m.extract_selective(package, stem, targets, out/'current'/stem)
    print(stem, len(targets), sorted(targets))
for stem in ('H.Texture_en', 'B.Texture_en'):
    c = m.Cache(package, stem)
    targets = {p for p in c.entries if 'Font' in p or 'font' in p}
    m.extract_selective(package, stem, targets, out/'current'/stem)
    print(stem, len(targets))
for p in (out/'current/H.Font').rglob('*'):
    if not p.is_file(): continue
    rel = p.relative_to(out/'current/H.Font')
    if '/Fonts/' not in rel.as_posix() and not rel.as_posix().startswith('Temp/FontCache/'):continue
    h = p.read_bytes()
    b = (out/'current/B.Font'/rel).read_bytes()
    # Every language has N*16 UV + N*28 metrics + N*2 codes.
    candidates=[]
    for n in range(50, 300):
        if n*46 > len(b): break
        codes = struct.unpack_from('<'+str(n)+'H', b, n*44)
        if all(x<y for x,y in zip(codes,codes[1:])) and 32 in codes and 65 in codes:
            candidates.append((n, sum(ord(c) in codes for c in 'ĄąĆćĘęŁłŃńÓóŚśŹźŻż'), [i for i in range(len(h)-3) if h[i:i+4]==struct.pack('<I',n)]))
    print(str(rel), 'H/B', len(h),len(b),'N/PL/Hoffsets',candidates)
