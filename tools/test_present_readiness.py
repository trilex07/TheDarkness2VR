import unittest
from check_present_readiness import assess

HEADER = '''[EffectiveConfig] DisableOpenXR=1 SubmitMode=4 StereoMode=0 StartupMode=2
[EffectiveConfig] OpenXR=disabled
[DiagnosticEnginePresent] installed=1 rollbackUncertain=0
[DiagnosticEnginePresent] identity sc=AB vt=CD device=EF caller=00A94057 exactCaller=1 tid=3
'''
ROW = '[DiagnosticEnginePresent] entries=1000 forwarded=1000 returned=1000 recursive=0 hr=00000000 sc=AB tid=3 fault=0 ok=1000 statuses=0 errors=0\n'


class PresentReadinessTests(unittest.TestCase):
    def test_all_returns_covered_by_counter_prefix(self):
        result = assess(HEADER + ROW)
        self.assertEqual(result['status'], 'PASS_OBSERVED_PREFIX')
        self.assertEqual(result['covered_returns'], 1000)
        self.assertFalse(result['vr_approved'])
        self.assertFalse(result['full_session_proven'])

    def test_legacy_log_is_not_upgraded_to_full_hr_proof(self):
        self.assertEqual(assess(HEADER + ROW.split(' ok=')[0])['status'], 'NO_GO')

    def test_hidden_intermittent_failure_remains_visible_at_next_sample(self):
        changed = ROW.replace('ok=1000 statuses=0 errors=0', 'ok=999 statuses=0 errors=1')
        self.assertEqual(assess(HEADER + changed)['status'], 'NO_GO')

    def test_occlusion_is_not_display_success(self):
        changed = ROW.replace('ok=1000 statuses=0', 'ok=999 statuses=1')
        self.assertEqual(assess(HEADER + changed)['status'], 'NO_GO')

    def test_inconsistent_identity_and_counts(self):
        for changed in (ROW.replace('tid=3', 'tid=4'), ROW.replace('forwarded=1000', 'forwarded=999'),
                        ROW.replace('ok=1000', 'ok=999'), ROW.replace('fault=0', 'fault=1'),
                        ROW.replace('recursive=0', 'recursive=1')):
            with self.subTest(changed=changed):
                self.assertEqual(assess(HEADER + changed)['status'], 'NO_GO')

    def test_empty_exception_and_wrong_mode(self):
        for text in ('', HEADER + ROW + '!!! EXCEPTION', (HEADER + ROW).replace('StereoMode=0', 'StereoMode=1')):
            self.assertEqual(assess(text)['status'], 'NO_GO')

    def test_counter_regression(self):
        self.assertEqual(assess(HEADER + ROW + ROW.replace('1000', '999'))['status'], 'NO_GO')


if __name__ == '__main__':
    unittest.main()
