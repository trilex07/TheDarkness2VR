#!/usr/bin/env python3
"""
Fast LANCZOS upscaling from 2K to 4K and 8K.
Used after Real-ESRGAN AI upscaling to 2K (which is slow on CPU).
LANCZOS from 2K is fast and preserves the AI-added detail.

Usage:
    python tools/upscale_lanczos.py --source extracted_textures --dest extracted_textures
"""

import os
import argparse
from pathlib import Path
from PIL import Image


def upscale_lanczos(src_path, dest_path, target_max):
    """Upscale a single image using LANCZOS interpolation."""
    img = Image.open(src_path).convert('RGB')
    w, h = img.size
    ratio = target_max / max(w, h)
    new_w = max(1, round(w * ratio))
    new_h = max(1, round(h * ratio))
    if max(w, h) >= target_max:
        # Already >= target, just resize down
        out = img.resize((new_w, new_h), Image.LANCZOS)
    else:
        out = img.resize((new_w, new_h), Image.LANCZOS)
    out.save(dest_path, 'PNG', compress_level=6)
    return (src_path, True, f'{w}x{h} -> {out.size[0]}x{out.size[1]}')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--source', required=True, help='root folder containing 2k/ subdirs')
    ap.add_argument('--dest', required=True, help='root folder for output 4k/ 8k/')
    args = ap.parse_args()

    source = Path(args.source)
    dest = Path(args.dest)

    for cat in sorted(os.listdir(source / '2k')):
        cat_src_2k = source / '2k' / cat
        if not cat_src_2k.is_dir():
            continue

        for target_k in [4, 8]:
            target_max = target_k * 1024
            cat_dest = dest / f'{target_k}k' / cat
            cat_dest.mkdir(parents=True, exist_ok=True)

            files = [f for f in sorted(os.listdir(cat_src_2k)) if f.endswith('.png')]
            print(f'\n=== {cat} -> {target_k}K: {len(files)} files ===')

            for f in files:
                src = cat_src_2k / f
                dst = cat_dest / f
                if dst.exists():
                    print(f'  SKIP {f}: already exists')
                    continue
                try:
                    _, ok, msg = upscale_lanczos(src, dst, target_max)
                    print(f'  OK {f}: {msg}')
                except Exception as e:
                    print(f'  FAIL {f}: {e}')


if __name__ == '__main__':
    main()
