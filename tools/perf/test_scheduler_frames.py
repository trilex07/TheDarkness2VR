import unittest
from analyze_scheduler_frames import analyze


HEADERS = [
 ['CSwitch','TimeStamp','New Process Name ( PID)','New TID','Old Process Name ( PID)','Old TID','OldState','Wait Reason'],
 ['ReadyThread','TimeStamp','Process Name ( PID)','ThreadID','Rdy Process Name ( PID)','Rdy TID'],
]


class SchedulerFrames(unittest.TestCase):
    def test_clips_running_blocked_and_ready_separately(self):
        rows = HEADERS + [
            ['CSwitch','10','game (7)','8','Idle (0)','0','Running','Executive'],
            ['CSwitch','20','Idle (0)','0','game (7)','8','Waiting','UserRequest'],
            ['ReadyThread','35','signal (9)','10','game (7)','8'],
            ['CSwitch','40','game (7)','8','Idle (0)','0','Running','Executive'],
            ['CSwitch','60','Idle (0)','0','game (7)','8','Waiting','UserRequest'],
        ]
        result = analyze(rows, [dict(start_us=15,stop_us=50)], 7, 8)[0]
        self.assertTrue(result['complete'])
        self.assertEqual((result['run_us'],result['off_cpu_us']), (15,20))
        self.assertEqual((result['gaps'][0]['blocked_us'],result['gaps'][0]['ready_us']), (15,5))
        self.assertEqual(result['gaps'][0]['ready_context'], ('signal (9)','10'))

    def test_foreign_thread_cannot_fill_missing_coverage(self):
        rows = HEADERS + [
            ['CSwitch','10','foreign (9)','8','Idle (0)','0','Running','Executive'],
            ['CSwitch','60','Idle (0)','0','foreign (9)','8','Waiting','UserRequest'],
        ]
        result = analyze(rows, [dict(start_us=15,stop_us=50)], 7, 8)[0]
        self.assertFalse(result['complete'])
        self.assertEqual(result['unknown_us'],35)

    def test_missing_ready_is_unknown_not_invented_blocking(self):
        rows = HEADERS + [
            ['CSwitch','20','Idle (0)','0','game (7)','8','Waiting','UserRequest'],
            ['CSwitch','40','game (7)','8','Idle (0)','0','Running','Executive'],
        ]
        result = analyze(rows, [dict(start_us=20,stop_us=40)], 7, 8)[0]
        self.assertTrue(result['complete'])
        self.assertIsNone(result['gaps'][0]['ready_us'])
        with self.assertRaises(ValueError):
            analyze(HEADERS + [['CSwitch','21']], [], 7, 8)


if __name__ == '__main__':
    unittest.main()
