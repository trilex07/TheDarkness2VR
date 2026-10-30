"""Polish font generator: filtered extractor input, native raw DXT3 output.

Draws real visual diacritics (acute, dot, ogonek, stroke) for all 18 Polish letters:
ĄąĆćĘęŁłŃńÓóŚśŹźŻż. Preserves all other languages (1..6) and native glyphs intact.
"""
from __future__ import annotations

import struct
import math
from pathlib import Path
import numpy as np

from polish_pack import Cache, require

# 18 Polish characters sorted by unicode codepoint:
PL_LETTERS = [
    ('Ó', 0x00d3, 'O', 'acute'),
    ('ó', 0x00f3, 'o', 'acute'),
    ('Ą', 0x0104, 'A', 'ogonek'),
    ('ą', 0x0105, 'a', 'ogonek'),
    ('Ć', 0x0106, 'C', 'acute'),
    ('ć', 0x0107, 'c', 'acute'),
    ('Ę', 0x0118, 'E', 'ogonek'),
    ('ę', 0x0119, 'e', 'ogonek'),
    ('Ł', 0x0141, 'L', 'stroke'),
    ('ł', 0x0142, 'l', 'stroke'),
    ('Ń', 0x0143, 'N', 'acute'),
    ('ń', 0x0144, 'n', 'acute'),
    ('Ś', 0x015a, 'S', 'acute'),
    ('ś', 0x015b, 's', 'acute'),
    ('Ź', 0x0179, 'Z', 'acute'),
    ('ź', 0x017a, 'z', 'acute'),
    ('Ż', 0x017b, 'Z', 'dot'),
    ('ż', 0x017c, 'z', 'dot'),
]
PL_LETTERS.sort(key=lambda x: x[1])

LETTERS = [(ch, donor) for ch, _, donor, _ in PL_LETTERS]

# Font specifications:
# (font_path, tex_path, count_offset, glyph_count, atlas_w, atlas_h, x_start, y_start, h_span)
FONTS = [
    ("EE/Menus/Fonts/SubtitlesFont", "Temp/EE/Menus/Fonts/SubtitlesFontGlyph_en_f.tga", 374, 97, 1024, 64, 0, 0, 0),
    ("EE/Menus/Fonts/MenuFont", "Temp/EE/Menus/Fonts/MenuFontGlyph_en_f.tga", 1262, 99, 1024, 128, 0, 0, 0),
    ("Temp/FontCache/darknessheader_19.syn", "Temp/Temp/FontCache/darknessheader_19.synGlyph_en_f.tga", 1048, 111, 1024, 64, 0, 0, 0),
    ("Temp/FontCache/darknessheader_24.syn", "Temp/Temp/FontCache/darknessheader_24.synGlyph_en_f.tga", 1048, 111, 1024, 64, 0, 0, 0),
    ("Temp/FontCache/darknessheader_26.syn", "Temp/Temp/FontCache/darknessheader_26.synGlyph_en_f.tga", 1048, 111, 1024, 64, 0, 0, 0),
    ("Temp/FontCache/darknessheader_32.syn", "Temp/Temp/FontCache/darknessheader_32.synGlyph_en_f.tga", 1048, 111, 1024, 128, 0, 0, 0),
]


def planar_to_native_dxt3(data: bytes) -> bytes:
    """Undo the cache compression filter before writing an uncompressed resource.

    Native 0x66C330 scatters each DWORD plane at a 16-byte block stride.
    Raw TOC entries bypass that filter (0xA552A0); DXT3 upload copies them
    directly (0xE120F0). Extractor output has NOT applied this unfilter.
    """
    require(len(data) > 0 and len(data) % 16384 == 0, 'DXT3 filter chunk length')
    source = np.frombuffer(data, dtype=np.uint8).reshape(-1, 4, 1024, 4)
    return source.transpose(0, 2, 1, 3).copy().tobytes()


def decode_native_dxt3(data: bytes, width: int, height: int) -> np.ndarray:
    """Independent Pillow BC2 decoder, exactly the raw bytes submitted to D3D9."""
    import io
    from PIL import Image
    require(width > 0 and height > 0 and width % 4 == height % 4 == 0,
            'DXT3 atlas dimensions')
    require(len(data) == width * height, 'DXT3 atlas byte count')
    # DDS_HEADER with a FOURCC DXT3 pixel format; no mipmaps or pitch padding.
    header = struct.pack('<7I11I8I5I', 124, 0x81007, height, width, len(data), 0, 0,
                         *([0]*11), 32, 4, 0x33545844, 0, 0, 0, 0, 0,
                         0x1000, 0, 0, 0, 0)
    with Image.open(io.BytesIO(b'DDS ' + header + data)) as image:
        return np.array(image.convert('RGBA'))


def decode_planar_dxt3(raw_bytes: bytes, width: int, height: int) -> np.ndarray:
    """Decodes 16KB-planar DXT3 into an 8-bit alpha map (height, width)."""
    bx_cnt = width // 4
    num_chunks = len(raw_bytes) // 16384
    alpha = np.zeros((height, width), dtype=np.uint8)
    for c in range(num_chunks):
        chunk = raw_bytes[c*16384 : (c+1)*16384]
        a_part = chunk[:8192]
        for b in range(1024):
            b_idx = c * 1024 + b
            by = b_idx // bx_cnt
            bx = b_idx % bx_cnt
            # The native compression filter separates the low/high DWORDs
            # of every alpha block, not complete eight-byte alpha blocks.
            a_data = a_part[b*4 : (b+1)*4] + a_part[4096+b*4 : 4096+(b+1)*4]
            for y in range(4):
                b0 = a_data[y*2]
                b1 = a_data[y*2 + 1]
                alpha[by*4 + y, bx*4 + 0] = (b0 & 0x0F) * 17
                alpha[by*4 + y, bx*4 + 1] = ((b0 >> 4) & 0x0F) * 17
                alpha[by*4 + y, bx*4 + 2] = (b1 & 0x0F) * 17
                alpha[by*4 + y, bx*4 + 3] = ((b1 >> 4) & 0x0F) * 17
    return alpha


def encode_planar_dxt3(alpha: np.ndarray, width: int, height: int, orig_raw: bytes) -> bytes:
    """Encodes modified alpha back into 16KB-planar DXT3."""
    baseline = decode_planar_dxt3(orig_raw, width, height)
    require(alpha.shape == baseline.shape, "Atlas dimensions changed")
    bx_cnt = width // 4
    by_cnt = height // 4
    num_chunks = len(orig_raw) // 16384
    out_chunks = [bytearray(orig_raw[c*16384 : (c+1)*16384]) for c in range(num_chunks)]
    for by in range(by_cnt):
        for bx in range(bx_cnt):
            b_idx = by * bx_cnt + bx
            c = b_idx // 1024
            b = b_idx % 1024
            sub_a = alpha[by*4 : (by+1)*4, bx*4 : (bx+1)*4]
            old_a = baseline[by*4 : (by+1)*4, bx*4 : (bx+1)*4]
            if not np.array_equal(sub_a, old_a):
                # Recoloring a block shared with an original glyph would alter
                # native pixels. New glyphs must occupy empty whole blocks.
                require(not np.any(old_a), "New glyph overlaps native atlas block")
                a_bytes = bytearray(8)
                for y in range(4):
                    a0 = int(round(sub_a[y, 0] / 17.0))
                    a1 = int(round(sub_a[y, 1] / 17.0))
                    a2 = int(round(sub_a[y, 2] / 17.0))
                    a3 = int(round(sub_a[y, 3] / 17.0))
                    a_bytes[y*2] = (min(15, max(0, a1)) << 4) | min(15, max(0, a0))
                    a_bytes[y*2 + 1] = (min(15, max(0, a3)) << 4) | min(15, max(0, a2))
                out_chunks[c][b*4 : (b+1)*4] = a_bytes[:4]
                out_chunks[c][4096+b*4 : 4096+(b+1)*4] = a_bytes[4:]
                out_chunks[c][8192 + b*4 : 8192 + (b+1)*4] = struct.pack('<HH', 0xFFFF, 0x0000)
                out_chunks[c][8192 + 4096 + b*4 : 8192 + 4096 + (b+1)*4] = struct.pack('<I', 0)
    return b"".join(out_chunks)


def draw_diacritic(composite: np.ndarray, accent: str, donor: str, top_y: int, bot_y: int,
                    left_x: int, right_x: int, y_offset: int, w: int, h: int) -> None:
    """Draws an anti-aliased Polish diacritic onto composite glyph."""
    mid_x = (left_x + right_x) // 2
    char_h = bot_y - top_y + 1
    kreska_len = max(3, min(7, int(char_h * 0.12)))
    kreska_thickness = 2 if char_h > 30 else 1

    if accent == 'acute':
        kreska_len = min(kreska_len, max(1, y_offset + top_y))
        start_y = max(0, y_offset + top_y - kreska_len)
        for i in range(kreska_len):
            cy = start_y + i
            cx = mid_x + (kreska_len // 2) - i
            for t in range(kreska_thickness):
                if 0 <= cy < h and 0 <= cx + t < w:
                    composite[cy, cx + t] = 255
                    if cx + t + 1 < w: composite[cy, cx + t + 1] = max(composite[cy, cx + t + 1], 180)
                    if cx + t - 1 >= 0: composite[cy, cx + t - 1] = max(composite[cy, cx + t - 1], 140)

    elif accent == 'dot':
        dot_r = 1 if char_h < 40 else 2
        dot_r = min(dot_r,max(0,(y_offset+top_y-2)//2))
        cy = max(dot_r,y_offset + top_y - dot_r - 2)
        cx = mid_x
        for dy in range(-dot_r, dot_r + 1):
            for dx in range(-dot_r, dot_r + 1):
                if dy*dy + dx*dx <= dot_r*dot_r + 1:
                    ny, nx = cy + dy, cx + dx
                    if 0 <= ny < h and 0 <= nx < w:
                        composite[ny, nx] = 255

    elif accent == 'ogonek':
        hook_len = max(3, min(6, int(char_h * 0.12)))
        start_y = y_offset + bot_y
        start_x = right_x - 1
        for i in range(hook_len):
            cy = min(h - 1, start_y + i)
            cx = min(w - 1, start_x + (i // 2))
            if 0 <= cy < h and 0 <= cx < w:
                composite[cy, cx] = 255
                if cx + 1 < w: composite[cy, cx + 1] = max(composite[cy, cx + 1], 180)

    elif accent == 'stroke':
        stroke_y = y_offset + (top_y + bot_y) // 2
        # Extend outside the stem: an accent inside thick ink disappears.
        for cx in range(max(0, left_x-2), min(w-1, right_x+2)+1):
            cy = stroke_y - ((cx-mid_x) // 3)
            if 0 <= cy < h and 0 <= cx < w:
                composite[cy, cx] = 255
                if cy + 1 < h: composite[cy + 1, cx] = max(composite[cy + 1, cx], 180)


def patch_font(header: bytes, body: bytes, count_offset: int, count: int,
               raw_tex: bytes | None = None,
               tex_meta: tuple[int, int, int, int, int] | None = None) -> tuple[bytes, bytes] | tuple[bytes, bytes, bytes]:
    """Patches font tables with real Polish diacritics.

    If raw_tex is provided, renders visual diacritics onto the atlas and updates exact UVs.
    If raw_tex is None, performs backward-compatible Unicode alias mapping.
    """
    counts = struct.unpack_from("<7I", header, count_offset)
    require(counts[0] == count and sum(counts)*46 <= len(body), "Font language layout")
    cursor = 0
    for length in counts:
        codes_in_language = struct.unpack_from("<"+str(length)+"H", body, cursor+length*44)
        require(all(a < b for a, b in zip(codes_in_language, codes_in_language[1:])), "Other font language layout")
        cursor += length*46
    require(struct.unpack_from("<7I", header, count_offset+32) == counts, "Font paired counts")
    codes = list(struct.unpack_from("<"+str(count)+"H", body, count*44))
    require(all(a < b for a, b in zip(codes, codes[1:])), "Original glyph code order")

    orig_uvs = [body[i*16:(i+1)*16] for i in range(count)]
    orig_metrics = [body[count*16+i*28:count*16+(i+1)*28] for i in range(count)]
    items = {code: (orig_uvs[i], orig_metrics[i]) for i, code in enumerate(codes)}
    original = dict(items)

    new_raw_tex = None

    if raw_tex is not None and tex_meta is not None:
        tw, th, x_start, y_start, h_span = tex_meta
        alpha = decode_planar_dxt3(raw_tex, tw, th)
        occupied = alpha.reshape(th//4, 4, tw//4, 4).any(axis=(1, 3))
        # Transparent padding is still sampled by the original glyph's quad.
        # Reserve every native UV rectangle, including spaces/punctuation.
        for i in range(count):
            u0,v0,u1,v1=struct.unpack_from('<4f',body,i*16)
            x0=max(0,math.floor(u0*tw)//4);y0=max(0,math.floor(v0*th)//4)
            x1=min(tw//4,math.ceil(math.ceil(u1*tw)/4));y1=min(th//4,math.ceil(math.ceil(v1*th)/4))
            occupied[y0:y1,x0:x1]=True

        for ch, code, donor, accent in PL_LETTERS:
            require(code not in items, f"Existing glyph conflict: {ch}")
            d_idx = codes.index(ord(donor))
            du0, dv0, du1, dv1 = struct.unpack_from('<4f', body, d_idx*16)
            dm = struct.unpack_from('<7f', body, count*16 + d_idx*28)

            dx0, dx1 = math.floor(du0 * tw), math.ceil(du1 * tw)
            dy0, dy1 = math.floor(dv0 * th), math.ceil(dv1 * th)
            dw = max(1, dx1 - dx0)
            dh = max(1, dy1 - dy0)

            donor_slice = np.zeros((dh, dw), dtype=np.uint8)
            sy0, sy1 = max(0, dy0), min(th, dy1)
            sx0, sx1 = max(0, dx0), min(tw, dx1)
            donor_slice[sy0-dy0:sy1-dy0, sx0-dx0:sx1-dx0] = alpha[sy0:sy1, sx0:sx1]
            extra_w = 2 if accent in ('ogonek', 'stroke', 'acute') else 1
            target_w = dw + extra_w

            h_span = dh
            bw, bh = (target_w+3)//4, (h_span+3)//4
            slot = next(((x, y) for y in range(occupied.shape[0]-bh+1)
                         for x in range(occupied.shape[1]-bw+1)
                         if not occupied[y:y+bh, x:x+bw].any()), None)
            require(slot is not None, "No unused whole atlas blocks for Polish glyph")
            bx, by = slot
            occupied[by:by+bh, bx:bx+bw] = True
            x_cursor, y_start = bx*4, by*4

            composite = np.zeros((h_span, target_w), dtype=np.uint8)
            d_nz = np.where(donor_slice > 25)
            if len(d_nz[0]) > 0:
                top_y, bot_y = d_nz[0].min(), d_nz[0].max()
                left_x, right_x = d_nz[1].min(), d_nz[1].max()
                # Preserve the donor's baseline; use its transparent top margin.
                y_offset = 0
                copy_h = min(dh, h_span - y_offset)
                composite[y_offset : y_offset + copy_h, :dw] = donor_slice[:copy_h, :]
                draw_diacritic(composite, accent, donor, top_y, bot_y, left_x, right_x, y_offset, target_w, h_span)

            target = alpha[y_start : y_start + h_span, x_cursor : x_cursor + target_w]
            require(not np.any(target), "Polish glyph overlaps existing atlas pixels")
            target[:] = composite

            u0 = x_cursor / float(tw)
            u1 = (x_cursor + target_w) / float(tw)
            v0 = y_start / float(th)
            v1 = (y_start + h_span) / float(th)

            bearing, base_ratio, s_x, s_y, u4, adv, u6 = dm
            new_adv = adv + (target_w - dw) * 0.5 if extra_w > 0 else adv
            new_metric = (bearing, base_ratio, s_x, s_y, u4, new_adv, u6)

            items[code] = (struct.pack('<4f', u0, v0, u1, v1),
                           struct.pack('<7f', *new_metric))
            x_cursor += target_w + 2

        new_raw_tex = encode_planar_dxt3(alpha, tw, th, raw_tex)
        require(len(new_raw_tex) == len(raw_tex), "Re-encoded texture length mismatch")
    else:
        for letter, donor in LETTERS:
            require(ord(letter) not in items, "Existing glyph conflict")
            items[ord(letter)] = original[ord(donor)]

    keys = sorted(items)
    n = len(keys)
    new_body = b"".join(items[k][0] for k in keys) + b"".join(items[k][1] for k in keys)
    new_body += struct.pack("<"+str(n)+"H", *keys) + body[count*46:]
    new_header = bytearray(header)
    for off in (count_offset, count_offset+32):
        struct.pack_into("<I", new_header, off, n)

    require(n == count+18 and new_body[n*46:] == body[count*46:], "Unrelated font language changed")
    require(all(items[k] == v for k, v in original.items()), "Original glyph changed")

    if new_raw_tex is not None:
        return bytes(new_header), new_body, new_raw_tex
    return bytes(new_header), new_body


def build_fonts(stage: Path):
    stage = Path(stage)
    has_tex = (stage / "baseline" / "B.Texture_en.cache").is_file()
    stem_list = ("B.Font", "H.Font", "B.Texture_en", "H.Texture_en") if has_tex else ("B.Font", "H.Font")
    caches = {s: Cache(stage / "baseline", s) for s in stem_list}
    patches = {s: {} for s in caches}
    report = []

    for item in FONTS:
        path, tex_path, offset, count, tw, th, xs, ys, hs = item
        h_data = (stage / "extract/H.Font" / path).read_bytes()
        b_data = (stage / "extract/B.Font" / path).read_bytes()
        tex_file = stage / "extract/B.Texture_en" / tex_path

        if has_tex and tex_file.is_file():
            raw_tex = tex_file.read_bytes()
            original_height = th
            if path.endswith('/MenuFont'):
                # The menu atlas has no room for 18 full-height glyph quads.
                # Add transparent rows; retain all native pixel coordinates.
                # Only English UV v coordinates scale with this English atlas.
                th *= 2
                original_body = b_data
                scaled = bytearray(b_data)
                for i in range(count):
                    u0,v0,u1,v1=struct.unpack_from('<4f',b_data,i*16)
                    struct.pack_into('<4f',scaled,i*16,u0,v0/2,u1,v1/2)
                b_data=bytes(scaled)
                require(b_data[count*16:]==original_body[count*16:], 'Menu metrics/other languages changed')
                raw_tex += bytes(tw*(th-original_height))
                ht=(stage/'extract/H.Texture_en'/tex_path).read_bytes()
                dimensions=struct.pack('<4H',tw,original_height,tw,original_height)
                require(ht.count(dimensions)==1,'Ambiguous menu texture dimensions')
                patches['H.Texture_en'][tex_path]=ht.replace(dimensions,struct.pack('<4H',tw,th,tw,th))
            h, b, t = patch_font(h_data, b_data, offset, count, raw_tex, (tw, th, xs, ys, hs))
            patches["H.Font"][path] = h
            patches["B.Font"][path] = b
            patches["B.Texture_en"][tex_path] = planar_to_native_dxt3(t)
            report.append({
                "font": path,
                "texture": tex_path,
                "glyphs": count + 18,
                "width": tw,
                "height": th,
                "mode": "authentic visual diacritics in native interleaved DXT3 raw atlas"
            })
        else:
            h, b = patch_font(h_data, b_data, offset, count)
            patches["H.Font"][path] = h
            patches["B.Font"][path] = b
            report.append({
                "font": path,
                "glyphs": count + 18,
                "mode": "original-Latin-glyph Unicode fallback; accents not drawn"
            })

    for stem, cache in caches.items():
        cache.write(stage / "candidate", patches[stem])

    return report
