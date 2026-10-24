"""PE evidence-reader tests. Synthetic bytes are parser fixtures, not engine proof."""
import struct
import unittest

from audit_camera import BASE, PE, audit


def fixture():
    b = bytearray(0x400)
    b[:2] = b"MZ"
    struct.pack_into("<I", b, 0x3C, 0x80)
    b[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<HHIIIHH", b, 0x84, 0x14C, 1, 0x4F68A875, 0, 0, 224, 0)
    struct.pack_into("<H", b, 0x98, 0x10B)
    struct.pack_into("<I", b, 0x98 + 28, BASE)
    struct.pack_into("<I", b, 0x98 + 56, 0xDEC000)
    b[0x178:0x180] = b".text\0\0\0"
    struct.pack_into("<IIII", b, 0x180, 0x300, 0x1000, 0x200, 0x200)
    b[0x210:0x214] = b"TEST"
    return b


class ReaderTests(unittest.TestCase):
    def test_va_uses_section_raw_offset_not_rva(self):
        self.assertEqual(PE(fixture()).at_va(BASE + 0x1010, 4), b"TEST")

    def test_virtual_zero_fill_is_not_file_evidence(self):
        with self.assertRaises(ValueError):
            PE(fixture()).at_va(BASE + 0x1200, 1)

    def test_section_crossing_is_rejected(self):
        with self.assertRaises(ValueError):
            PE(fixture()).at_va(BASE + 0x11FF, 2)

    def test_matching_pe_header_does_not_establish_identity(self):
        result = audit(fixture())
        self.assertFalse(result["known_identity"])
        self.assertFalse(result["all_sites_match"])
        self.assertFalse(result["native_cpu_frustum_proven"])

    def test_truncation_at_every_header_boundary(self):
        for size in (0, 1, 2, 0x3D, 0x80, 0x84, 0x98, 0x178, 0x19F, 0x3FF):
            with self.subTest(size=size), self.assertRaises(ValueError):
                PE(fixture()[:size])

    def test_invalid_pe_and_optional_header(self):
        for off, value in ((0, 0), (0x80, 0), (0x98, 0x20B)):
            blob = fixture()
            struct.pack_into("<H", blob, off, value)
            with self.subTest(off=off), self.assertRaises(ValueError):
                PE(blob)


if __name__ == "__main__":
    unittest.main()
