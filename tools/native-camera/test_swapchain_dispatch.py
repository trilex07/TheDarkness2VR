"""Synthetic guard tests, not a replacement for the failed real Steam test."""
import hashlib
import struct
import unittest
from unittest.mock import patch
import audit_swapchain_dispatch as subject
from test_audit_camera import fixture


class SwapchainEvidenceTests(unittest.TestCase):
    def synthetic_runtime(self):
        blob = fixture()
        blob[0x210:0x210+len(subject.PRIVATE_CALL_BYTES)] = subject.PRIVATE_CALL_BYTES
        return blob

    def test_public_contract_is_shorter_than_observed_private_dispatch(self):
        displacement = struct.unpack_from("<I", subject.PRIVATE_CALL_BYTES, 5)[0]
        self.assertEqual(displacement, 0x270)
        self.assertEqual(displacement // 4, 156)
        self.assertGreaterEqual(displacement // 4, 134)
        self.assertEqual(subject.PRIVATE_CALL_BYTES[-2:], b"\xff\xd6")

    def test_unknown_file_cannot_prove_private_slot(self):
        result = subject.runtime_evidence(fixture())
        self.assertFalse(result["verified"])
        self.assertIsNone(result["private_slot"])
        self.assertFalse(result["public_length_shadow_safe"])

    def test_matching_instructions_without_identity_stay_unknown(self):
        blob = self.synthetic_runtime()
        with patch.object(subject, "PRIVATE_CALL_RVA", 0x1010):
            result = subject.runtime_evidence(blob)
        self.assertTrue(result["instruction_match"])
        self.assertFalse(result["known_identity"])
        self.assertIsNone(result["private_slot"])

    def test_verified_private_slot_is_not_complete_table_extent_or_hook_permission(self):
        blob = self.synthetic_runtime()
        with patch.multiple(subject, PRIVATE_CALL_RVA=0x1010,
                            D3D9_SHA=hashlib.sha256(blob).hexdigest(), D3D9_BASE=0x400000,
                            D3D9_TIMESTAMP=0x4F68A875, D3D9_SIZE=0xDEC000):
            result = subject.runtime_evidence(blob)
        self.assertTrue(result["verified"])
        self.assertEqual(result["private_slot"], 156)
        self.assertIsNone(result["complete_native_vtable_extent"])
        self.assertFalse(result["public_length_shadow_safe"])

    def test_instruction_change_fails_even_with_explicit_identity(self):
        blob = self.synthetic_runtime()
        blob[0x215] ^= 1
        with patch.multiple(subject, PRIVATE_CALL_RVA=0x1010,
                            D3D9_SHA=hashlib.sha256(blob).hexdigest(), D3D9_BASE=0x400000,
                            D3D9_TIMESTAMP=0x4F68A875, D3D9_SIZE=0xDEC000):
            result = subject.runtime_evidence(blob)
        self.assertTrue(result["known_identity"])
        self.assertFalse(result["verified"])
        self.assertIsNone(result["private_slot"])

    def test_synthetic_game_does_not_prove_acquisition_path(self):
        result = subject.engine_evidence(fixture())
        self.assertFalse(result["verified"])
        self.assertFalse(any(r["match"] for r in result["ranges"]))


if __name__ == "__main__":
    unittest.main()
