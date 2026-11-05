"""Check native bytecode identities and paired model->clip/model->mask paths."""
import argparse
import hashlib
import json
import re
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('archive', type=Path)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
header = (Path(__file__).resolve().parents[1] / 'src/lit_mask_projection.h').read_text()
results = []
for key, size, digest in re.findall(r'\{0x([0-9A-F]{8})u,(\d+),"([0-9a-f]{64})"\}', header):
    blob = (a.archive / 'shaders' / f'vs-{key}.bin').read_bytes()
    asm = (a.archive / 'asm' / f'vs-{key}.txt').read_text()
    mask = re.search(r'mul (r\d+), (?:c9, (\w+)\.y|(\w+)\.y, c9)\s+'
                     r'mad \1, (\w+)\.x, c8, \1\s+'
                     r'mad \1, (\w+)\.z, c10, \1\s+add o3, \1, c11', asm)
    point = (mask[2] or mask[3]) if mask else 'INVALID'
    # Intervening skinning instructions may calculate other components; the
    # three input components and accumulation registers must still agree.
    raster = re.search(r'mul (r\d+), (?:c1, '+point+r'\.y|'+point+r'\.y, c1)\s+'
                       r'.*?mad (r\d+), '+point+r'\.x, c0, \1\s+'
                       r'mad \2, '+point+r'\.z, c2, \2\s+add o0, \2, c3', asm, re.S)
    checks = dict(length=len(blob) == int(size), sha256=hashlib.sha256(blob).hexdigest() == digest,
                  nested='} LitInstance;' in asm and 'ScreenTextureProjection;' in asm,
                  ctab=bool(re.search(r'LitInstance\s+c0\s+1[2-7]', asm)),
                  texcoord='dcl_texcoord2 o3' in asm, mask=bool(mask), raster=bool(raster),
                  same_point=bool(mask and point == mask[4] == mask[5]))
    results.append(dict(vs=key, checks=checks, passed=all(checks.values())))
ps = (a.archive / 'asm/ps-4BC661BA.txt').read_text()
assert re.search(r'ShadowMask\s+s3', ps) and 'texldp_pp r0, v2, s3' in ps
a.output.parent.mkdir(parents=True, exist_ok=True)
a.output.write_text(json.dumps(results, indent=2))
assert results and all(r['passed'] for r in results), 'Native evidence mismatch; inspect report'
print(f'PASS {len(results)} exact LitInstance identities and shared-point mask/raster data flows')

