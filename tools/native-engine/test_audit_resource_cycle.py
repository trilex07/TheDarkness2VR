"""Parser/admission tests for the offline auditor, not native engine equivalence."""
import json
from pathlib import Path
import tempfile
import unittest
import audit_resource_cycle as audit


class AuditTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.addCleanup(self.tmp.cleanup)
        self.header = 'Executable SHA256: ' + audit.EXPECTED_SHA + '\n'
        for name in audit.EXPORTS:
            (self.root/name).write_text(self.header + 'EVIDENCE COMPLETE\n')
        body = ''.join(f'ADDRESS {entry}\nFUNCTION {entry} inferred\n{entry} PUSH EBP\n'
                       for entry in audit.FUNCTIONS)
        (self.root/audit.EXPORTS[0]).write_text(self.header + body + 'EVIDENCE COMPLETE\n')
        self.sample = self.root/'samples.json'
        self.payload = {'partial': False, 'exit': 0, 'phases': [['combat', 1, 2]],
            'pcs': [['combat', 7, 'DarknessII.exe!0x0000000000ac36f0', 3],
                    ['combat', 7, 'DarknessII.exe!0x0000000000400000', 5],
                    ['combat', 8, 'DarknessII.exe!0x0000000000ac36f0', 99],
                    ['combat', 7, 'ntdll.dll!Unknown', 20]]}

    def count(self, **kwargs):
        self.sample.write_text(json.dumps(self.payload))
        owners, _ = audit.read_exports(self.root)
        args = dict(tid=7, phase='combat', presents=2)
        args.update(kwargs)
        return audit.count_samples(self.sample, owners, **args)

    def test_unknown_native_preserved_and_threads_not_combined(self):
        result = self.count()
        self.assertEqual(result['native_leaf_samples'], 8)
        self.assertEqual(result['cycle_leaf_samples'], 3)
        self.assertEqual(result['unclassified_native_leaf_samples'], 5)
        self.assertEqual(result['cycle_leaf_estimated_ms_per_present'], 1.5)

    def test_incomplete_export_rejected(self):
        (self.root/audit.EXPORTS[1]).write_text(self.header)
        with self.assertRaisesRegex(ValueError, 'Incomplete'):
            audit.read_exports(self.root)

    def test_wrong_export_identity_rejected(self):
        (self.root/audit.EXPORTS[1]).write_text('Executable SHA256: wrong\nEVIDENCE COMPLETE\n')
        with self.assertRaisesRegex(ValueError, 'identity'):
            audit.read_exports(self.root)

    def test_conflicting_owner_rejected(self):
        (self.root/audit.EXPORTS[1]).write_text(self.header +
            'FUNCTION 006f0190 inferred\n00ac36f0 PUSH EBP\nEVIDENCE COMPLETE\n')
        with self.assertRaisesRegex(ValueError, 'Conflicting'):
            audit.read_exports(self.root)

    def test_missing_bodies_rejected(self):
        (self.root/audit.EXPORTS[0]).write_text(self.header + 'EVIDENCE COMPLETE\n')
        with self.assertRaisesRegex(ValueError, 'Missing'):
            audit.read_exports(self.root)

    def test_partial_and_failed_extraction_rejected(self):
        for key, value in [('partial', True), ('exit', 1)]:
            with self.subTest(key=key):
                old = self.payload[key]
                self.payload[key] = value
                with self.assertRaisesRegex(ValueError, 'incomplete'):
                    self.count()
                self.payload[key] = old

    def test_wrong_phase_thread_denominator_rejected(self):
        for kwargs in [{'phase': 'indoor'}, {'tid': 99}, {'presents': 0}]:
            with self.subTest(kwargs=kwargs), self.assertRaises(ValueError):
                self.count(**kwargs)

    def test_negative_weight_rejected(self):
        self.payload['pcs'][0][3] = -1
        with self.assertRaisesRegex(ValueError, 'weight'):
            self.count()

    def test_output_cannot_replace_evidence_or_executable(self):
        source = self.root/'input.json'
        source.write_text('evidence')
        with self.assertRaisesRegex(ValueError, 'overwrite'):
            audit.validate_output(source, [source])
        with self.assertRaisesRegex(ValueError, '.json'):
            audit.validate_output(self.root/'game.exe', [source])
        self.assertEqual(source.read_text(), 'evidence')

    def test_wrong_exe_rejected_before_address_interpretation(self):
        p = self.root/'not-a-game.exe'
        p.write_bytes(b'not executable')
        with self.assertRaisesRegex(ValueError, 'SHA256'):
            audit.audit_image(p)


if __name__ == '__main__':
    unittest.main()
