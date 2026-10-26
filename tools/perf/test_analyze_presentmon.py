import csv
import json
import tempfile
import unittest
from pathlib import Path

from analyze_presentmon import analyze, percentile


class PresentMonAnalysisTest(unittest.TestCase):
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


if __name__ == "__main__":
    unittest.main()
