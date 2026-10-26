"""PE evidence-reader tests. Synthetic bytes are parser fixtures, not engine proof."""
import hashlib
import struct
import unittest
from unittest.mock import patch

import audit_camera

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

    def test_matching_instruction_evidence_does_not_accept_unlisted_file(self):
        # Synthetic parser evidence only. Matching all selected code must not
        # replace the exact full-file identity gate (including non-code bytes).
        sites = {"fixture": (BASE + 0x1010, b"TEST".hex())}
        ranges = {BASE + 0x1010: (4, hashlib.sha256(b"TEST").hexdigest())}
        with patch.dict(audit_camera.SITES, sites, clear=True), patch.dict(audit_camera.FUNCTION_RANGES, ranges, clear=True):
            result = audit(fixture())
        self.assertTrue(result["all_sites_match"])
        self.assertTrue(result["all_function_ranges_match"])
        self.assertFalse(result["known_identity"])
        self.assertIsNone(result["identity_label"])

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


class FrameCapReaderTests(unittest.TestCase):
    def reader(self, mode=0, limit=60.0):
        fields = bytearray(48)
        struct.pack_into("<I", fields, 0, mode)
        struct.pack_into("<f", fields, 44, limit)
        def read(va, count):
            if va in audit_camera.FRAME_CAP_SITES:
                return audit_camera.FRAME_CAP_SITES[va]
            self.assertEqual((va,count),(0x010EB068,48))
            return bytes(fields)
        return read

    def test_current_fields_are_not_a_pacing_claim(self):
        for mode, cap in ((0,60.0),(1,90.0),(7,0.0)):
            value=audit_camera.frame_cap_snapshot(self.reader(mode,cap))
            self.assertEqual(value["frame_cap_mode"],mode)
            self.assertEqual(value["max_frame_rate"],cap)
            self.assertEqual(value["effective_pacing"],"UNKNOWN")

    def test_nonfinite_value_stays_unknown_json_safe(self):
        value=audit_camera.frame_cap_snapshot(self.reader(0,float("nan")))
        self.assertIsNone(value["max_frame_rate"])

    def test_instruction_mismatch_stops_before_field_reads(self):
        reads=[]
        def wrong(va,count):
            reads.append(va)
            return bytes(count)
        with self.assertRaisesRegex(ValueError,"instruction mismatch"):
            audit_camera.frame_cap_snapshot(wrong)
        self.assertEqual(len(reads),1)

    def test_changed_or_short_fields_are_rejected(self):
        original=self.reader()
        for short in (True,False):
            reads=0
            def changing(va,count):
                nonlocal reads
                b=original(va,count)
                if va==0x010EB068:
                    reads+=1
                    if reads==2:return b[:-1] if short else bytes(48)
                return b
            with self.assertRaisesRegex(ValueError,"changed or read was incomplete"):
                audit_camera.frame_cap_snapshot(changing)


if __name__ == "__main__":
    unittest.main()
