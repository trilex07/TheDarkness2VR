"""Offline regression for the TWO recorded E129EA pointers; never executes a sample.

Synthetic tests need no game. --exe admits only the exact audited Steam SHA/PE,
then hashes its range tables read-only. Mutations below touch local bytearrays.
No original image, dump, decompilation or trace is written or redistributed.
"""
from __future__ import annotations
import argparse
import hashlib
import struct
import unittest
import zlib
from pathlib import Path
from audit_camera import PE

SHA = "d46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1"
MASK = 0xffffffff
# table, size table, count, base-relative word, encoded source, caller return
PROFILES = {
    "A": (0x1035ec8, 0x10360a0, 113, 0x10369c0, 0x10369f4, 0x667cc4),
    "B": (0x104a928, 0x104ab60, 137, 0x104b660, 0x104b674, 0x8b7825),
}
EXE: Path | None = None


def shuffle(x: int, adjacent: bool = False) -> int:
    # 833F70; 632EC0 additionally exchanges adjacent bits at its tail.
    pairs = [(8, 0xff00), (4, 0xf000f0), (2, 0xc0c0c0c), (1, 0x22222222)]
    if adjacent:
        pairs.append((1, 0x55555555))
    for shift, mask in pairs:
        t = ((x >> shift) ^ x) & mask
        x = (x ^ t ^ (t << shift)) & MASK
    return x


def key(name: str, crc: int) -> int:
    if name == "A":
        rotated = (crc >> 1) | ((crc & 1) << 31)
        return shuffle(rotated) ^ 0x03000300
    # 897800 calls 632EC0, then 6F4C30 (full bit reversal).
    reversed_bits = int(f"{shuffle(crc, True):032b}"[::-1], 2)
    return reversed_bits ^ 0x07070707


def decode(word_va: int, encoded: int, transformed: int) -> int:
    return ((encoded ^ transformed) + word_va) & MASK


def admit(blob: bytes) -> PE:
    if hashlib.sha256(blob).hexdigest() != SHA:
        raise ValueError("EXE SHA mismatch: no model is run on an unknown image")
    pe = PE(blob)
    if (pe.machine, pe.timestamp, pe.base, pe.image_size) != (0x14c, 0x4f68a875, 0x400000, 0xdec000):
        raise ValueError("EXE PE mismatch")
    return pe


def reproduce(pe: PE, name: str, patches=()) -> tuple[int, int, int, list[int]]:
    table, sizes, count, base_word, word, caller = PROFILES[name]
    u32 = lambda va: struct.unpack("<I", pe.at_va(va, 4))[0]
    base = (base_word + u32(base_word)) & MASK
    if base != pe.base:
        raise ValueError("unexpected relocation delta")
    crc, overlaps = 0, []
    for index in range(count):
        entry = table + 4 * index
        start, size = (entry + u32(entry)) & MASK, u32(sizes + 4 * index)
        chunk = bytearray(pe.at_va(start, size))
        for target, replacement in patches:
            lo, hi = max(start, target), min(start + size, target + len(replacement))
            if lo < hi:
                chunk[lo-start:hi-start] = replacement[lo-target:hi-target]
                overlaps.append(index)
        crc = zlib.crc32(chunk, crc)
    crc = zlib.crc32(struct.pack("<I", caller - base), crc)
    transformed = key(name, crc)
    return crc, transformed, decode(word, u32(word), transformed), overlaps


class ArithmeticTests(unittest.TestCase):
    def test_native_transforms(self):
        for name, crc, expected in [("A", 0x0058cdf0, 0x17145ee0), ("A", 0x7bed2d9c, 0x48b6f97c),
                                    ("B", 0x00e0b6c0, 0x07281356), ("B", 0x64eaf96c, 0x20394e7a)]:
            with self.subTest(name=name, crc=crc):
                self.assertEqual(key(name, crc), expected)

    def test_relative_add_wraps_at_32_bits(self):
        self.assertEqual(decode(0xfffffff0, 0x20, 0), 0x10)

    def test_unknown_image_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "SHA mismatch"):
            admit(b"untrusted sample")


@unittest.skipUnless(EXE, "supply --exe for read-only pinned-image regression")
class RecordedCrashTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.pe = admit(EXE.read_bytes())

    def test_pristine_sources(self):
        self.assertEqual(reproduce(self.pe, "A"), (0x0058cdf0, 0x17145ee0, 0x00f36a00, []))
        self.assertEqual(reproduce(self.pe, "B"), (0x00e0b6c0, 0x07281356, 0x00755c00, []))

    def test_a_global_resource_vtable_patch_alone_matches_crash(self):
        self.assertEqual(self.pe.at_va(0xfc2664, 4), struct.pack("<I", 0x6c3890))
        patch = (0xfc2664, struct.pack("<I", 0x55f20070))
        self.assertEqual(reproduce(self.pe, "A", [patch]), (0x7bed2d9c, 0x48b6f97c, 0xa1561184, [32]))

    def test_b_aim_call_patch_alone_matches_crash(self):
        self.assertEqual(self.pe.at_va(0xc63009, 6), bytes.fromhex("8b829c000000"))
        patch = (0xc63009, b"\xb8" + struct.pack("<I", 0x55f1faa0) + b"\x90")
        self.assertEqual(reproduce(self.pe, "B", [patch]), (0x64eaf96c, 0x20394e7a, 0xd966af14, [66]))

    def test_restore_original_inputs_restores_valid_sources(self):
        # Rollback must restore bytes, never replace the native hash/decoder.
        for name, target, size in [("A", 0xfc2664, 4), ("B", 0xc63009, 6)]:
            self.assertEqual(reproduce(self.pe, name, [(target, self.pe.at_va(target, size))])[:3],
                             reproduce(self.pe, name)[:3])


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path)
    args, rest = parser.parse_known_args()
    EXE = args.exe
    # unittest's class decorator ran before argparse; update this one gate.
    RecordedCrashTests.__unittest_skip__ = not bool(EXE)
    unittest.main(argv=[__file__, *rest], verbosity=2)
