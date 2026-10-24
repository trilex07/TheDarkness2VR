import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

from check_matched_capture import FILES, validate


class MatchedCaptureTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.folder = Path(self.temp.name)
        self.records = [{"kind": "header", "frame": 7, **{k: "a" * 64 for k in
                         ("dll_sha256", "exe_sha256", "ini_sha256", "request_sha256")}}]
        self.config = {"eye_params": [{"head_geometry": False}, {"head_geometry": False}]}
        for eye in (0, 1):
            self.records.append(dict(kind="draw", frame=7, eye=eye, draw=0, bank_offset=eye*4096))
            self.records.append(dict(kind="source", frame=7, eye=eye, width=8, height=8, viewport=[0,0,8,8,0,1]))
            self.records.append(dict(kind="input",frame=7,eye=eye,rt="test-rt"))
            self.records[-2]["rt"]="test-rt"
            cb = [0]*24
            cb[17:20] = [3, 1, 1]
            self.records.append(dict(kind="blit", frame=7, eye=eye, cb_floats=cb, viewport=[0,0,8,8,0,1]))
        for eye in (0, 1):
            self.records.append(dict(kind="final", frame=7, eye=eye, width=8, height=8))
            self.records.append(dict(kind="submission", frame=7, eye=eye, fov_l_r_u_d=[-.5,.5,.5,-.5]))
            for kind in ("source", "final"):
                pixels = b"".join(bytes((x*24,y*24,80,255)) for y in range(8) for x in range(8))
                bmp = struct.pack("<2sIHHI", b"BM", 54+len(pixels),0,0,54)
                bmp += struct.pack("<IiiHHIIiiII",40,8,-8,1,32,0,len(pixels),0,0,0,0)
                (self.folder/f"{kind}-eye{eye}.bmp").write_bytes(bmp+pixels)
        (self.folder/"vs-banks.bin").write_bytes(bytes(8192))
        (self.folder/"effective-config.json").write_text(json.dumps(self.config))
        digest = hashlib.sha256((self.folder/"effective-config.json").read_bytes()).hexdigest()
        self.records.extend([dict(kind="config_identity",frame=7,effective_config_sha256=digest),
                             dict(kind="result",frame=7,complete=True,mask=15,draws=[1,1])])
        self.save()

    def save(self):
        (self.folder/"manifest.jsonl").write_text("\n".join(json.dumps(r) for r in self.records)+"\n")
        receipt = {name: hashlib.sha256((self.folder/name).read_bytes()).hexdigest() for name in FILES}
        (self.folder/"COMPLETE.json").write_text(json.dumps(receipt))

    def test_complete(self):
        self.assertEqual(validate(self.folder)["frame"], 7)

    def test_mixed_frame(self):
        self.records[1]["frame"] = 8
        self.save()
        with self.assertRaisesRegex(ValueError, "mixed frame"):
            validate(self.folder)

    def test_missing_eye(self):
        self.records = [r for r in self.records if not(r["kind"]=="final" and r["eye"]==1)]
        self.save()
        with self.assertRaisesRegex(ValueError, "missing or duplicate"):
            validate(self.folder)

    def test_duplicate_eye(self):
        self.records.insert(-1, self.records[2])
        self.save()
        with self.assertRaisesRegex(ValueError, "missing or duplicate"):
            validate(self.folder)

    def test_overwritten_source(self):
        source = self.records.pop(2)
        self.records.insert(7, source)
        self.save()
        with self.assertRaisesRegex(ValueError, "boundary ordering|after replay"):
            validate(self.folder)

    def test_partial_gpu_readback(self):
        self.records[-1]["complete"] = False
        self.save()
        with self.assertRaisesRegex(ValueError, "runtime rejected"):
            validate(self.folder)

    def test_truncated_bank(self):
        (self.folder/"vs-banks.bin").write_bytes(bytes(8191))
        self.save()
        with self.assertRaisesRegex(ValueError, "truncated"):
            validate(self.folder)

    def test_tampered_image(self):
        (self.folder/"source-eye0.bmp").write_bytes(b"bad")
        with self.assertRaisesRegex(ValueError, "hash mismatch"):
            validate(self.folder)

    def test_interrupted_capture(self):
        (self.folder/"COMPLETE.json").unlink()
        with self.assertRaises(OSError):
            validate(self.folder)

    def test_luna_ray_compare(self):
        command = [sys.executable,str(Path(__file__).with_name("check_matched_capture.py")),str(self.folder),
                   "--eye","0","--source-fov-tan","-.54630249",".54630249",".54630249","-.54630249",
                   "--camera-proof-note","synthetic symmetric projection"]
        result = subprocess.run(command,capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertIn("synthetic symmetric projection",result.stdout)


if __name__ == "__main__":
    unittest.main()
