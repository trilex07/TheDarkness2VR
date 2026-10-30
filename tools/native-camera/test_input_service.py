"""Reject wrong native ownership chains without touching a running process."""
import unittest

from audit_input_service import observe_chain


class InputOwnershipTests(unittest.TestCase):
    def setUp(self):
        self.memory = {
            0x10000: 0xFC8930, 0x100F8: 0x5678, 0x100A8: 0x20000,
            0x20000: 0x30000, 0x30374: 0x40000,
            0x40000: 0x50000, 0x50000: 0xFBFCD0,
        }

    def read(self, address):
        if address not in self.memory:
            raise ValueError("unreadable snapshot")
        return self.memory[address]

    def test_reference_indirections_are_required(self):
        result = observe_chain(self.read, 0x5678, 0x10000)
        self.assertEqual(result["input_object"], 0x50000)
        # Treating either reference as its object must not produce a receipt.
        for field, value in ((0x100A8, 0x30000), (0x30374, 0x50000)):
            with self.subTest(field=field):
                original = self.memory[field]
                self.memory[field] = value
                with self.assertRaises(ValueError):
                    observe_chain(self.read, 0x5678, 0x10000)
                self.memory[field] = original

    def test_wrong_window_and_dynamic_types_rejected(self):
        with self.assertRaises(ValueError):
            observe_chain(self.read, 0x1234, 0x10000)
        for field in (0x10000, 0x50000):
            original = self.memory[field]
            self.memory[field] = 0xFBFADC  # unknown neighboring type is insufficient
            with self.assertRaises(ValueError):
                observe_chain(self.read, 0x5678, 0x10000)
            self.memory[field] = original

    def test_observed_windows_base_preserves_reference_layout(self):
        derived = observe_chain(self.read, 0x5678, 0x10000)
        self.memory[0x50000] = 0xFBFAD8
        base = observe_chain(self.read, 0x5678, 0x10000)
        self.assertEqual(base["input_object"], derived["input_object"])
        self.assertEqual(base["input_table"], 0xFBFAD8)
        self.assertNotEqual(base, derived)
        for unknown in (0, 0xFBFAD4, 0xFBFADC, 0xFBFCCC, 0xFBFCD4):
            self.memory[0x50000] = unknown
            with self.assertRaises(ValueError):
                observe_chain(self.read, 0x5678, 0x10000)

    def test_invalid_reference_values_rejected(self):
        for address in (0x100A8, 0x20000, 0x30374, 0x40000):
            original = self.memory[address]
            for invalid in (0, True, 0xFFFF, 0x20001, 0x100000000):
                with self.subTest(address=address, invalid=invalid):
                    self.memory[address] = invalid
                    with self.assertRaises(ValueError):
                        observe_chain(self.read, 0x5678, 0x10000)
            self.memory[address] = original

    def test_field_overflow_and_missing_memory_rejected(self):
        self.memory[0xFFFFFFFC] = 0xFC8930
        with self.assertRaisesRegex(ValueError, "overflow"):
            observe_chain(self.read, 0x5678, 0xFFFFFFFC)
        del self.memory[0x40000]
        with self.assertRaisesRegex(ValueError, "unreadable"):
            observe_chain(self.read, 0x5678, 0x10000)


if __name__ == "__main__":
    unittest.main()
