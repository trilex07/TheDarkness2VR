#!/usr/bin/env python3
"""
Convert raw Evolution Engine texture blocks (extracted by QuickBMS from toc/cache)
into PNG images.

Strategy: for each source file, try multiple interpretations:
  - Format: DXT1, DXT5, RGBA8, RGB8
  - Dimensions: common power-of-two sizes (64..1024)
  - Header offset: 0, 4, 8, 16, 32 bytes (possible EEngine header)
  - Single level or full mipmap chain

DXT1 / DXT5 decompression is implemented in pure Python (no external deps).
Uncompressed RGBA8/RGB8 decoded directly with Pillow.

Usage:
    python tools/convert_raw_textures.py --source extracted_textures/source --dest extracted_textures/converted
"""

import os
import struct
import argparse
from pathlib import Path

from PIL import Image


# ---------------------------------------------------------------------------
# DXT1 / DXT5 block decompression (pure Python)
# ---------------------------------------------------------------------------
def _unpack565(data):
    """Unpack a 16-bit R5G6B5 color to (r, g, b) 0-255."""
    r = (data >> 11) & 0x1F
    g = (data >> 5) & 0x3F
    b = data & 0x1F
    return (r * 255 // 31, g * 255 // 63, b * 255 // 31)


def _interp565(c0, c1, factor):
    """Linear interpolate between two R5G6B5 colors (factor 0..1, out of 3)."""
    r = c0[0] + (c1[0] - c0[0]) * factor // 3
    g = c0[1] + (c1[1] - c0[1]) * factor // 3
    b = c0[2] + (c1[2] - c0[2]) * factor // 3
    return (r, g, b)


def _dxt1_colors(c0_raw, c1_raw):
    """Return 4 RGBA colors for a DXT1 block."""
    c0 = _unpack565(c0_raw)
    c1 = _unpack565(c1_raw)
    if c0_raw > c1_raw:
        return [
            (c0[0], c0[1], c0[2], 255),
            (c1[0], c1[1], c1[2], 255),
            _interp565(c0, c1, 1) + (255,),
            _interp565(c0, c1, 2) + (255,),
        ]
    else:
        return [
            (c0[0], c0[1], c0[2], 255),
            (c1[0], c1[1], c1[2], 255),
            ((c0[0] + c1[0]) // 2, (c0[1] + c1[1]) // 2, (c0[2] + c1[2]) // 2, 255),
            (0, 0, 0, 0),
        ]


def _dxt5_alphas(a0, a1, index_bits):
    """Return 8 alpha values for a DXT5 block."""
    if a0 > a1:
        alphas = [a0, a1]
        for i in range(1, 7):
            alphas.append((a0 * (7 - i) + a1 * i) // 6)
    else:
        alphas = [a0, a1]
        for i in range(1, 5):
            alphas.append((a0 * (5 - i) + a1 * i) // 4)
        alphas += [0, 255]
    indices = []
    bits = index_bits
    for _ in range(16):
        indices.append(bits & 0x7)
        bits >>= 3
    return [alphas[i] for i in indices]


def decompress_dxt1_block(data, offset):
    c0_raw = struct.unpack_from('<H', data, offset)[0]
    c1_raw = struct.unpack_from('<H', data, offset + 2)[0]
    indices_raw = struct.unpack_from('<I', data, offset + 4)[0]
    colors = _dxt1_colors(c0_raw, c1_raw)
    result = []
    bits = indices_raw
    for row in range(4):
        for col in range(4):
            idx = bits & 0x3
            bits >>= 2
            result.append(colors[idx])
    return result


def decompress_dxt5_block(data, offset):
    a0 = data[offset]
    a1 = data[offset + 1]
    alpha_bits = (
        data[offset + 2]
        | (data[offset + 3] << 8)
        | (data[offset + 4] << 16)
        | (data[offset + 5] << 24)
        | (data[offset + 6] << 32)
        | (data[offset + 7] << 40)
    )
    alphas = _dxt5_alphas(a0, a1, alpha_bits)
    c0_raw = struct.unpack_from('<H', data, offset + 8)[0]
    c1_raw = struct.unpack_from('<H', data, offset + 10)[0]
    indices_raw = struct.unpack_from('<I', data, offset + 12)[0]
    colors = _dxt1_colors(c0_raw, c1_raw)
    result = []
    bits = indices_raw
    for i in range(16):
        idx = bits & 0x3
        bits >>= 2
        r, g, b, _ = colors[idx]
        result.append((r, g, b, alphas[i]))
    return result


def decompress_dxt1_level(data, offset, width, height):
    blocks_w = (width + 3) // 4
    blocks_h = (height + 3) // 4
    img = Image.new('RGBA', (width, height))
    px = img.load()
    for by in range(blocks_h):
        for bx in range(blocks_w):
            block = decompress_dxt1_block(data, offset)
            offset += 8
            for py in range(4):
                for px_ in range(4):
                    x = bx * 4 + px_
                    y = by * 4 + py
                    if x < width and y < height:
                        px[x, y] = block[py * 4 + px_]
    return img, offset


def decompress_dxt5_level(data, offset, width, height):
    blocks_w = (width + 3) // 4
    blocks_h = (height + 3) // 4
    img = Image.new('RGBA', (width, height))
    px = img.load()
    for by in range(blocks_h):
        for bx in range(blocks_w):
            block = decompress_dxt5_block(data, offset)
            offset += 16
            for py in range(4):
                for px_ in range(4):
                    x = bx * 4 + px_
                    y = by * 4 + py
                    if x < width and y < height:
                        px[x, y] = block[py * 4 + px_]
    return img, offset


def dxt1_level_size(w, h):
    return ((w + 3) // 4) * ((h + 3) // 4) * 8


def dxt5_level_size(w, h):
    return ((w + 3) // 4) * ((h + 3) // 4) * 16


def dxt1_chain_size(w, h):
    total = 0
    while w >= 1 and h >= 1:
        total += dxt1_level_size(w, h)
        w = max(1, w // 2)
        h = max(1, h // 2)
        if w == 1 and h == 1:
            break
    return total


def dxt5_chain_size(w, h):
    total = 0
    while w >= 1 and h >= 1:
        total += dxt5_level_size(w, h)
        w = max(1, w // 2)
        h = max(1, h // 2)
        if w == 1 and h == 1:
            break
    return total


def decompress_dxt1_chain(data, offset, width, height):
    w, h = width, height
    img = Image.new('RGBA', (width, height))
    px = img.load()
    while w >= 1 and h >= 1:
        level_img, offset = decompress_dxt1_level(data, offset, w, h)
        # Blit level onto main image (level 0 is full size, smaller mipmaps are ignored
        # for output but we still consume their bytes)
        if w == width and h == height:
            img = level_img
            px = img.load()
        w = max(1, w // 2)
        h = max(1, h // 2)
        if w == 1 and h == 1:
            break
    return img, offset


def decompress_dxt5_chain(data, offset, width, height):
    w, h = width, height
    img = Image.new('RGBA', (width, height))
    px = img.load()
    while w >= 1 and h >= 1:
        level_img, offset = decompress_dxt5_level(data, offset, w, h)
        if w == width and h == height:
            img = level_img
            px = img.load()
        w = max(1, w // 2)
        h = max(1, h // 2)
        if w == 1 and h == 1:
            break
    return img, offset


# ---------------------------------------------------------------------------
# Conversion
# ---------------------------------------------------------------------------
DIMS = [
    (64, 64), (128, 64), (64, 128), (128, 128),
    (256, 128), (128, 256), (256, 256),
    (512, 256), (256, 512), (512, 512),
    (1024, 512), (512, 1024), (1024, 1024),
]

HEADER_OFFSETS = [0, 4, 8, 16, 32]


def convert_file(src_path):
    with open(src_path, 'rb') as f:
        data = f.read()

    size = len(data)
    if size == 0:
        return None, 'empty file'

    # Try uncompressed formats first
    for offset in HEADER_OFFSETS:
        if offset >= size:
            continue
        payload = data[offset:]
        plen = len(payload)
        for w, h in DIMS:
            if w * h * 4 == plen:
                try:
                    img = Image.frombytes('RGBA', (w, h), payload[:w * h * 4], 'raw')
                    return img, f'RGBA8 {w}x{h} (offset={offset})'
                except Exception:
                    pass
            if w * h * 3 == plen:
                try:
                    img = Image.frombytes('RGB', (w, h), payload[:w * h * 3], 'raw')
                    return img, f'RGB8 {w}x{h} (offset={offset})'
                except Exception:
                    pass

    # Try compressed formats (single level or mipmap chain)
    for offset in HEADER_OFFSETS:
        if offset >= size:
            continue
        payload = data[offset:]
        plen = len(payload)

        for w, h in DIMS:
            # DXT5 single level
            if dxt5_level_size(w, h) == plen:
                try:
                    img, _ = decompress_dxt5_level(payload, 0, w, h)
                    return img, f'DXT5 {w}x{h} (offset={offset})'
                except Exception:
                    pass
            # DXT5 mipmap chain
            if dxt5_chain_size(w, h) == plen:
                try:
                    img, _ = decompress_dxt5_chain(payload, 0, w, h)
                    return img, f'DXT5 {w}x{h}+mip (offset={offset})'
                except Exception:
                    pass

            # DXT1 single level
            if dxt1_level_size(w, h) == plen:
                try:
                    img, _ = decompress_dxt1_level(payload, 0, w, h)
                    return img, f'DXT1 {w}x{h} (offset={offset})'
                except Exception:
                    pass
            # DXT1 mipmap chain
            if dxt1_chain_size(w, h) == plen:
                try:
                    img, _ = decompress_dxt1_chain(payload, 0, w, h)
                    return img, f'DXT1 {w}x{h}+mip (offset={offset})'
                except Exception:
                    pass

    return None, f'no format matched ({size} bytes)'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--source', required=True, help='source folder with category subdirs')
    ap.add_argument('--dest', required=True, help='destination folder for converted PNGs')
    args = ap.parse_args()

    source = Path(args.source)
    dest = Path(args.dest)
    dest.mkdir(parents=True, exist_ok=True)

    stats = {'ok': 0, 'fail': 0, 'by_format': {}}

    for cat in sorted(os.listdir(source)):
        catdir = source / cat
        if not catdir.is_dir():
            continue

        cat_dest = dest / cat
        cat_dest.mkdir(parents=True, exist_ok=True)

        files = [f for f in sorted(os.listdir(catdir)) if f.endswith('.png')]
        print(f'\n=== {cat}: {len(files)} files ===')

        for f in files:
            src = catdir / f
            img, desc = convert_file(src)
            if img is not None:
                out = cat_dest / f
                img.save(out, 'PNG')
                stats['ok'] += 1
                stats['by_format'][desc] = stats['by_format'].get(desc, 0) + 1
                print(f'  OK {f}: {desc} -> {img.size}')
            else:
                stats['fail'] += 1
                print(f'  FAIL {f}: {desc}')

    print(f'\n=== Conversion complete ===')
    print(f'OK: {stats["ok"]}, FAIL: {stats["fail"]}')
    print('By format:')
    for k, v in sorted(stats['by_format'].items(), key=lambda x: -x[1]):
        print(f'  {k}: {v}')


if __name__ == '__main__':
    main()
