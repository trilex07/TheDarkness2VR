import csv
import json
import tempfile
import unittest
from pathlib import Path

from analyze_presentmon import analyze, percentile, normalize_row


class PresentMonAnalysisTest(unittest.TestCase):
    def test_legacy_names_keep_cpu_and_present_intervals_distinct(self):
        row = normalize_row({"CPUStartQPCTimeInMs": "91072068.9035",
                             "MsBetweenAppStart": "12", "MsBetweenPresents": "99",
                             "MsCPUBusy": "10", "MsCPUWait": "2", "MsGPUBusy": "3"})
        self.assertEqual(row["FrameTime"], "12")
        self.assertEqual(row["CPUStartQPCTime"], "91072068.9035")
        self.assertEqual(row["CPUBusy"], "10")
        self.assertEqual(row["GPUBusy"], "3")
        self.assertEqual(row["MsBetweenPresents"], "99")

    def test_legacy_normalization_does_not_guess_units_or_replace_v2(self):
        row = normalize_row({"CPUStartQPC": "1234", "TimeInSeconds": "1.2",
                             "MsBetweenPresents": "99"})
        self.assertNotIn("CPUStartQPCTime", row)
        self.assertNotIn("FrameTime", row)
        row = normalize_row({"FrameTime": "NA", "MsBetweenAppStart": "12"})
        self.assertEqual(row["FrameTime"], "NA")

    def test_interpolated_percentile(self):
        self.assertEqual(percentile([10, 40, 20, 30], 50), 25)
        self.assertAlmostEqual(percentile([10, 20, 30, 40], 95), 38.5)

    def test_exact_pid_and_qpc_phase_split(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parent) as folder:
            root = Path(folder)
            manifest = {
                "valid": True, "processIds": {"game": 42}, "qpcFrequency": 1000,
                "baselineQpc": 1000, "combatQpc": 2000, "endQpc": 3000,
                "etwLostEvents": 0, "presentmonLostEvents": 0,
            }
            (root / "manifest.json").write_text(json.dumps(manifest), encoding="utf-8")
            with (root / "presentmon.csv").open("w", newline="", encoding="utf-8") as file:
                writer = csv.DictWriter(file, fieldnames=(
                    "ProcessID", "CPUStartQPCTime", "FrameTime", "CPUBusy", "CPUWait",
                    "GPUBusy", "GPUWait", "GPUTime", "GPULatency", "DisplayedTime"))
                writer.writeheader()
                for pid, time, frame in ((42, 1100, 10), (42, 1900, 20),
                                         (42, 2100, 30), (42, 2900, 40),
                                         (99, 2200, 999)):
                    writer.writerow(dict(ProcessID=pid, CPUStartQPCTime=time, FrameTime=frame,
                                         CPUBusy=1, CPUWait=2, GPUBusy=3, GPUWait=4,
                                         GPUTime=5, GPULatency=6, DisplayedTime="NA"))
            output = analyze(root)
            self.assertEqual(output["rows_total"], 4)
            self.assertEqual(output["baseline"]["rows"], 2)
            self.assertEqual(output["combat"]["rows"], 2)
            self.assertEqual(output["baseline"]["metrics_ms"]["FrameTime"]["p50"], 15)
            self.assertEqual(output["combat"]["metrics_ms"]["FrameTime"]["p95"], 39.5)
            self.assertEqual(output["combat"]["displayed_time_available"], 0)

    def test_outdoor_phase_edges_and_pid_are_not_mixed(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parent) as folder:
            root = Path(folder)
            phases = [{'id': name, 'startQpc': start, 'endQpc': start+100}
                      for name, start in [('indoor',1000), ('transition',1200), ('outdoor',1400), ('combat',1600)]]
            manifest = {'valid':True, 'processIds':{'game':42}, 'qpcFrequency':1000,
                        'baselineQpc':1000, 'combatQpc':1600, 'endQpc':1700,
                        'phases':phases, 'etwLostEvents':0, 'presentmonLostEvents':0}
            (root/'manifest.json').write_text(json.dumps(manifest))
            with (root/'presentmon.csv').open('w',newline='') as file:
                writer=csv.DictWriter(file,fieldnames=('ProcessID','CPUStartQPCTime','FrameTime'))
                writer.writeheader()
                for pid,t,ms in ((42,1010,10),(42,1090,20),(42,1150,1),(42,1210,15),
                                 (42,1410,25),(42,1610,35),(42,1690,15),(99,1610,70)):
                    writer.writerow(dict(ProcessID=pid,CPUStartQPCTime=t,FrameTime=ms))
            result=analyze(root)
            self.assertEqual(list(result['phases']),['indoor','transition','outdoor','combat'])
            self.assertEqual([v['rows'] for v in result['phases'].values()],[1,1,1,1])
            self.assertEqual(result['baseline']['metrics_ms']['FrameTime']['mean'],10)
            self.assertEqual(result['combat']['frames_over_ms']['33.33'],1)
            worst=result['worst_ten_outdoor_frames']
            self.assertEqual([v['phase'] for v in worst],['combat','outdoor'])
            self.assertEqual((worst[0]['start_qpc'],worst[0]['end_qpc']),(1610,1645))
            manifest['phases'][-1]['endQpc']=None
            (root/'manifest.json').write_text(json.dumps(manifest))
            with self.assertRaisesRegex(ValueError,'Incomplete phase: combat'):
                analyze(root)

    def test_large_absolute_qpc_uses_frequency_and_keeps_marker_gaps(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parent) as folder:
            root=Path(folder)
            frequency=10_000_000
            start=2_941_436_226_650
            phases=[{'id':name,'startQpc':start+offset*frequency,'endQpc':start+(offset+1)*frequency}
                    for name,offset in [('indoor',0),('transition',2),('outdoor',4),('combat',6)]]
            manifest={'valid':True,'processIds':{'game':42},'qpcFrequency':frequency,
                      'baselineQpc':start,'combatQpc':start+6*frequency,'endQpc':start+7*frequency,
                      'phases':phases,'etwLostEvents':0,'presentmonLostEvents':0}
            (root/'manifest.json').write_text(json.dumps(manifest))
            with (root/'presentmon.csv').open('w',newline='') as file:
                writer=csv.DictWriter(file,fieldnames=('ProcessID','CPUStartQPCTime','FrameTime'))
                writer.writeheader()
                # Raw marker/cue-gap row must remain in rows_total, but must not
                # be silently reclassified as indoor or outdoor steady work.
                for offset,ms in [(0.1,10),(1.5,150),(4.1,30),(6.1,40)]:
                    writer.writerow(dict(ProcessID=42,CPUStartQPCTime=(start+offset*frequency)*1000/frequency,FrameTime=ms))
            result=analyze(root)
            self.assertEqual(result['rows_total'],4)
            self.assertEqual(sum(v['rows'] for v in result['phases'].values()),3)
            self.assertEqual(result['worst_ten_outdoor_frames'][0]['start_qpc'],start+61*frequency//10)


if __name__ == "__main__":
    unittest.main()
