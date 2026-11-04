import unittest
from check_matched_capture import validate_selected_draw


class SelectedDrawTests(unittest.TestCase):
    def setUp(self):
        self.request = dict(kind='selected_draw_request', ps_hash=123, ordinal=1)
        self.before = dict(kind='pass_stage', phase='selected_before_draw', eye=0,
                           last_draw=7, saved=True, ps_hash=123, sampler=-1, rt='target',
                           vs_hash=99, format=113, width=1024, height=1024, file='before.bin')
        self.after = dict(self.before, phase='selected_after_draw', file='after.bin')
        self.result = dict(kind='selected_draw_return', draw=7, eye=0, hr=0,
                           target_hr=0, before_rt='target', after_rt='target', ps_hash=123)
        self.records = [self.request, dict(kind='draw_ps', eye=0, draw=2, shader_hash=123),
                        self.before, dict(kind='draw_ps', eye=0, draw=7, shader_hash=123),
                        self.result, self.after]

    def check(self):
        positions = {id(r): i for i, r in enumerate(self.records)}
        validate_selected_draw(self.records, lambda r: positions[id(r)])

    def test_actual_second_matching_draw(self):
        self.check()

    def test_foreign_target_rejected(self):
        self.result['after_rt'] = 'foreign'
        with self.assertRaisesRegex(ValueError, 'mismatched'): self.check()

    def test_failed_draw_rejected(self):
        self.result['hr'] = -1
        with self.assertRaisesRegex(ValueError, 'mismatched'): self.check()

    def test_missing_after_rejected(self):
        self.records.remove(self.after)
        with self.assertRaisesRegex(ValueError, 'incomplete'): self.check()

    def test_wrong_ordinal_rejected(self):
        self.request['ordinal'] = 0
        with self.assertRaisesRegex(ValueError, 'mismatched'): self.check()

    def test_reused_image_rejected(self):
        self.after['file'] = self.before['file']
        with self.assertRaisesRegex(ValueError, 'mismatched'): self.check()

    def test_after_precedes_draw_return_rejected(self):
        self.records[-1], self.records[-2] = self.records[-2], self.records[-1]
        with self.assertRaisesRegex(ValueError, 'mismatched'): self.check()

    def test_ordinary_capture_unaffected(self):
        validate_selected_draw([], lambda r: 0)


if __name__ == '__main__':
    unittest.main()
