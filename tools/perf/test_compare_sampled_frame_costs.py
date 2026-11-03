"""Timeline attribution checks with independently specified frame/sample data."""
import csv
import json
import pathlib
import subprocess
import sys
import tempfile
import unittest

SCRIPT = pathlib.Path(__file__).with_name("compare_sampled_frame_costs.py")


class AttributionTests(unittest.TestCase):
    def run_fixture(self, *, partial=False, overlap=False):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            samples, frames, start = [], [], 0
            # Two known populations: 60 frames at 10ms with weight1, then
            # 60 at 20ms with weight3. A boundary sample belongs to the NEXT
            # frame; repeated recursive callers count only once inclusively.
            for i in range(120):
                duration, weight = (10, 1) if i < 60 else (20, 3)
                frames.append([7, start / 1000, duration])
                samples.append(["combat", 9, "DarknessII.exe!0x00401001",
                                weight, start,
                                ["DarknessII.exe!0x00401001",
                                 "DarknessII.exe!0x00402001",
                                 "DarknessII.exe!0x00402001"]])
                # Foreign thread must never contribute.
                samples.append(["combat", 8, "foreign!work", 100, start, []])
                start += duration * 1000
            if overlap:
                frames[1][1] -= 1
            (root / "cpu-raw-all.json").write_text(json.dumps(dict(
                partial=partial, exit=0, phases=[["combat", 0, start]], samples=samples)))
            (root / "qpc-origin.json").write_text(json.dumps(dict(origin_qpc=0, frequency=10000000)))
            (root / "owners.csv").write_text(
                "pc,weight,entry,name\n00401001,1,00401000,leaf\n"
                "00402001,1,00402000,parent\nEVIDENCE COMPLETE\n")
            with (root / "presentmon.csv").open("w", newline="") as stream:
                writer = csv.writer(stream)
                writer.writerow(["ProcessID", "CPUStartQPCTime", "FrameTime"])
                writer.writerows(frames)
            result = subprocess.run([sys.executable, str(SCRIPT), str(root),
                                     "--owners", str(root / "owners.csv"),
                                     "--pid", "7", "--tid", "9", "--output",
                                     str(root / "result.json")], capture_output=True, text=True)
            output = root / "result.json"
            return result.returncode, json.loads(output.read_text()) if output.exists() else None

    def test_population_boundary_thread_and_recursive_stack(self):
        code, result = self.run_fixture()
        self.assertEqual(code, 0)
        self.assertEqual(result["assigned_weight"], 240)
        self.assertEqual(result["outside_selected_frames_weight"], 0)
        for group, duration, weight in (("fast", 10, 1), ("slow", 20, 3)):
            self.assertEqual(result[group]["mean_frame_ms"], duration)
            self.assertEqual(result[group]["native"], {"00401000": weight})
            self.assertEqual(result[group]["native_inclusive"],
                             {"00401000": weight, "00402000": weight})

    def test_partial_export_rejected(self):
        self.assertNotEqual(self.run_fixture(partial=True)[0], 0)

    def test_overlapping_frames_rejected(self):
        self.assertNotEqual(self.run_fixture(overlap=True)[0], 0)


if __name__ == "__main__":
    unittest.main()
