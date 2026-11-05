"""Check effective raster/mask matrix agreement, not headset visual acceptance."""
import argparse
import json
import re
import struct
from pathlib import Path

def objects(text):
    decoder = json.JSONDecoder()
    while text.strip():
        text = text.lstrip()
        row, end = decoder.raw_decode(text)
        yield row
        text = text[end:]

def inspect(path, allowed):
    rows = list(objects((path / 'manifest.jsonl').read_text()))
    header = next(r for r in rows if r['kind'] == 'header')
    bank = (path / 'vs-banks.bin').read_bytes()
    samples = []
    for r in rows:
        if r['kind'] != 'draw' or r['shader_hash'] not in allowed:
            continue
        w = struct.unpack_from('<16f', bank, r['bank_offset'])
        mask = struct.unpack_from('<16f', bank, r['bank_offset'] + 128)
        expected = [value for row in range(4) for value in
                    (.5*(w[row*4]+w[row*4+3]), .5*(-w[row*4+1]+w[row*4+3]),
                     w[row*4+2], w[row*4+3])]
        error = max(abs(x-y) for x, y in zip(mask, expected))
        # Native STP includes a render-target half-texel bias. The existing
        # unbias/head/rebias path rotates that small remainder too. This bound
        # allows it and rejects the old untracked-mask error (several units).
        relative = error / max(1, max(map(abs, w)))
        samples.append(dict(eye=r['eye'], draw=r['draw'], vs=f"{r['shader_hash']:08X}",
                            error=error, relative=relative, passed=relative < .001))
    return dict(path=str(path), dll=header['dll_sha256'], samples=samples,
                passed=bool(samples) and {s['eye'] for s in samples} == {0, 1}
                and all(s['passed'] for s in samples))

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('captures', type=Path, nargs='+')
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
h = (Path(__file__).resolve().parents[1] / 'src/lit_mask_projection.h').read_text()
allowed = {int(k, 16) for k in re.findall(r'\{0x([0-9A-F]{8})u,', h)}
results = [inspect(path, allowed) for path in a.captures]
a.output.write_text(json.dumps(results, indent=2))
assert all(r['passed'] for r in results), 'Mask/raster mismatch or missing eye; inspect report'
print(f'PASS effective mask/raster alignment: {sum(len(r["samples"]) for r in results)} draws, both eyes')
