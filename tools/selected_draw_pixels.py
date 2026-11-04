"""Visualize one validated draw's exact pixel contribution; not a performance test."""
import argparse
import json
from pathlib import Path
import numpy as np
from PIL import Image
from check_matched_capture import validate


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    validate(args.capture)
    records = [json.loads(line) for line in (args.capture/'manifest.jsonl').read_text().splitlines()]
    stages = {r['phase']: r for r in records if r['kind'] == 'pass_stage' and
              r.get('phase', '').startswith('selected_')}
    images = []
    for phase in ('selected_before_draw', 'selected_after_draw'):
        r = stages[phase]
        if r['format'] != 113:
            raise ValueError('This visualization requires RGBA16F, exact native channels preserved')
        images.append(np.fromfile(args.capture/r['file'], dtype='<f2').reshape(r['height'],r['width'],4).astype(np.float32))
    if not all(np.isfinite(im).all() for im in images):
        raise ValueError('Non-finite captured pixels')
    delta = np.max(np.abs(images[1]-images[0]), axis=2)
    changed = delta > 0
    args.output.mkdir(parents=True, exist_ok=True)
    for name, im in zip(('before','after'), images):
        # Diagnostic display transform only; raw capture remains untouched.
        rgb = np.maximum(im[:,:,:3], 0)
        rgb = np.power(rgb/(1+rgb), 1/2.2)
        display = Image.fromarray(np.uint8(np.clip(rgb*255,0,255)))
        display.thumbnail((1000,1000))
        display.save(args.output/f'{name}.png')
    mask = Image.fromarray(np.uint8(changed)*255)
    mask.thumbnail((1000,1000))
    mask.save(args.output/'changed-mask.png')
    result = dict(changed_pixels=int(changed.sum()), total_pixels=int(changed.size),
                  fraction=float(changed.mean()), max_channel_delta=float(delta.max()),
                  note='Exact within-frame draw contribution; no temporal stability or FPS claim')
    (args.output/'pixels.json').write_text(json.dumps(result,indent=2))
    print(json.dumps(result))


if __name__ == '__main__':
    main()
