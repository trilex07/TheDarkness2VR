"""Safe Unicode fallback in native font tables; no texture-format assumptions.

Accented Polish codepoints deliberately use the matching original Latin glyph.
This avoids missing text while retaining all atlas bytes and native metrics.
It is a fallback, not an implementation of visually accented glyphs.
"""
import struct
from polish_pack import Cache, require

LETTERS = list(zip("ÓóĄąĆćĘęŁłŃńŚśŹźŻż", "OoAaCcEeLlNnSsZzZz"))
FONTS = [("EE/Menus/Fonts/SubtitlesFont", 374, 97),
         ("EE/Menus/Fonts/MenuFont", 1262, 99)] + [
             ("Temp/FontCache/darknessheader_"+str(n)+".syn", 1048, 111)
             for n in (19, 24, 26, 32)]


def patch_font(header, body, count_offset, count):
    counts = struct.unpack_from("<7I", header, count_offset)
    require(counts[0] == count and sum(counts)*46 <= len(body), "Font language layout")
    cursor = 0
    for length in counts:
        codes_in_language = struct.unpack_from("<"+str(length)+"H", body, cursor+length*44)
        require(all(a < b for a, b in zip(codes_in_language, codes_in_language[1:])), "Other font language layout")
        cursor += length*46
    require(struct.unpack_from("<7I", header, count_offset+32) == counts, "Font paired counts")
    codes = struct.unpack_from("<"+str(count)+"H", body, count*44)
    require(all(a < b for a, b in zip(codes, codes[1:])), "Original glyph code order")
    items = {}
    for i, code in enumerate(codes):
        items[code] = (body[i*16:(i+1)*16], body[count*16+i*28:count*16+(i+1)*28])
    original = dict(items)
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
    return bytes(new_header), new_body


def build_fonts(stage):
    caches = {s: Cache(stage/"baseline", s) for s in ("B.Font", "H.Font")}
    patches = {s: {} for s in caches}
    report = []
    for path, offset, count in FONTS:
        h, b = patch_font((stage/"extract/H.Font"/path).read_bytes(),
                          (stage/"extract/B.Font"/path).read_bytes(), offset, count)
        patches["H.Font"][path], patches["B.Font"][path] = h, b
        report.append({"font": path, "glyphs": count+18, "mode": "original-Latin-glyph Unicode fallback; accents not drawn"})
    for stem, cache in caches.items():
        cache.write(stage/"candidate", patches[stem])
    return report
