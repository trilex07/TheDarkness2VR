import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

from check_matched_capture import FILES, validate, validate_shadow_conversion


class ShadowConversionTests(unittest.TestCase):
    def setUp(self):
        self.producer=dict(kind="depth_attachment",eye=0,draw=3,surface="depth",format=0x5A544E49,
                           enabled=1,write=1,depth_hr=0,desc_hr=0,enable_hr=0,write_hr=0)
        self.restore=dict(kind="shadow_copy_constants_restore",draw=9,valid=True,ps_equal=True,vs_equal=True)
        self.stage=dict(kind="pass_stage",last_draw=9,phase="before_draw_input",ps_hash=0xC4D1FB10,
                        sampler=0,saved=True,rt="copy",format=114,width=512,height=512)
        self.copy=dict(kind="shadow_depth_copy",eye=0,draw=9,status="saved",hr=0,source="depth",target="copy")
        self.identity=dict(kind="post_input",draw=9,ps_hash=0xC4D1FB10,sampler=0,surface="depth",
                           format=0x5A544E49,readback_status="saved",width=512,height=512)
        self.records=[self.producer,self.restore,self.stage,self.copy,self.identity]

    def check(self):
        positions={id(r):i for i,r in enumerate(self.records)}
        validate_shadow_conversion(self.records,[self.identity],[self.stage],lambda r:positions[id(r)])

    def test_exact_source_consumer_restore_and_producer(self):
        self.check()

    def test_foreign_source_or_target_rejected(self):
        self.copy["source"]="foreign"
        with self.assertRaisesRegex(ValueError,"mismatched"):self.check()
        self.copy["source"]="depth";self.stage["rt"]="foreign"
        with self.assertRaisesRegex(ValueError,"mismatched"):self.check()

    def test_changed_constant_bank_rejected(self):
        self.restore["ps_equal"]=False
        with self.assertRaisesRegex(ValueError,"changed constants"):self.check()

    def test_read_only_attachment_is_not_producer(self):
        self.producer["write"]=0
        with self.assertRaisesRegex(ValueError,"missing native"):self.check()

    def test_producer_after_consumer_rejected(self):
        self.producer["draw"]=10
        with self.assertRaisesRegex(ValueError,"missing native"):self.check()

    def test_wrong_copy_order_rejected(self):
        self.records[1],self.records[2]=self.records[2],self.records[1]
        with self.assertRaisesRegex(ValueError,"conversion order"):self.check()

    def test_missing_restore_or_duplicate_copy_rejected(self):
        self.records.remove(self.restore)
        with self.assertRaisesRegex(ValueError,"incomplete"):self.check()
        self.records.append(dict(self.copy))
        with self.assertRaisesRegex(ValueError,"duplicate"):self.check()


class MatchedCaptureTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.folder = Path(self.temp.name)
        self.records = [{"kind": "header", "frame": 7, "schema":2, **{k: "a" * 64 for k in
                         ("dll_sha256", "exe_sha256", "ini_sha256", "request_sha256")}}]
        self.records.append(dict(kind="capture_arm",frame=7,expected_serial=12,epoch=1,expected_game_frame=9,
                                 thread=3,reset_generation=2,device="test-dev"))
        self.config = {"eye_params": [{"head_geometry": False}, {"head_geometry": False}]}
        for eye in (0, 1):
            self.records.append(dict(kind="draw", frame=7, eye=eye, draw=0, bank_offset=eye*4096))
            if eye==0:
                self.records.append(dict(kind="present_entry",frame=7,serial=12,epoch=1,game_frame=9,
                                         thread=3,reset_generation=2,device="test-dev",boundary="DevicePresent"))
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
                             dict(kind="staged_result",frame=7,complete=True),
                             dict(kind="present_completion",frame=7,serial=12,epoch=1,game_frame=9,same_witness=True,forwarded=True,hr=0),
                             dict(kind="result",frame=7,complete=True,mask=15,draws=[1,1])])
        self.save()

    def save(self):
        (self.folder/"manifest.jsonl").write_text("\n".join(json.dumps(r) for r in self.records)+"\n")
        receipt = {name: hashlib.sha256((self.folder/name).read_bytes()).hexdigest() for name in FILES}
        (self.folder/"COMPLETE.json").write_text(json.dumps(receipt))

    def test_complete(self):
        self.assertEqual(validate(self.folder)["frame"], 7)

    def test_cpu_blit_parameters_can_precede_shared_input(self):
        # Production BuildStereoCBData records parameters before the shared
        # capture; neither this event nor its name witnesses a GPU Draw.
        for eye in (0, 1):
            blit = next(r for r in self.records if r["kind"] == "blit" and r["eye"] == eye)
            self.records.remove(blit)
            i = next(i for i,r in enumerate(self.records) if r["kind"] == "input" and r["eye"] == eye)
            self.records.insert(i, blit)
        self.save()
        self.assertEqual(validate(self.folder)["frame"], 7)

    def test_input_or_parameters_after_final_are_rejected(self):
        for kind in ("input", "blit"):
            event = next(r for r in self.records if r["kind"] == kind and r["eye"] == 0)
            old_index = self.records.index(event)
            self.records.remove(event)
            i = next(i for i,r in enumerate(self.records) if r["kind"] == "final" and r["eye"] == 0)
            self.records.insert(i+1, event)
            self.save()
            with self.assertRaisesRegex(ValueError, "invalid image/metadata boundary ordering"):
                validate(self.folder)
            self.records.remove(event)
            self.records.insert(old_index, event)

    def test_effective_lighting_constants_are_draw_scoped(self):
        witness = dict(kind="lighting_ps_constants", frame=7, eye=0, draw=0,
                       shader_hash=0xC4D1FB10, inverse_register=27, extents_register=-1,
                       valid=True, values=[0.0]*896)
        i = next(i for i,r in enumerate(self.records) if r["kind"] == "draw")
        self.records.insert(i+1, dict(kind="draw_ps", frame=7, eye=0, draw=0, shader_hash=0xC4D1FB10))
        self.records.insert(i+2, witness)
        self.save()
        self.assertEqual(validate(self.folder)["frame"], 7)
        for key, value in (("valid", False), ("shader_hash", 1), ("draw", 99),
                           ("inverse_register", 221), ("extents_register", 224),
                           ("values", [0.0]*895), ("values", [float('inf')]*896)):
            old = witness[key]
            witness[key] = value
            self.save()
            with self.assertRaisesRegex(ValueError, "invalid lighting PS constant witness"):
                validate(self.folder)
            witness[key] = old
        self.records.remove(witness)
        self.records.insert(-3, witness)
        self.save()
        with self.assertRaisesRegex(ValueError, "invalid lighting PS constant witness"):
            validate(self.folder)

    def test_optional_hand_palette_witness_keeps_frame_identity(self):
        witness = dict(kind="avatar_palette_overlap", frame=7, eye=0, draw=0,
                       status="compared", unique_matches=3, right_pose="survived")
        first_draw = next(i for i, record in enumerate(self.records) if record["kind"] == "draw")
        self.records.insert(first_draw + 1, witness)
        self.save()
        self.assertEqual(validate(self.folder)["frame"], 7)
        witness["frame"] = 8
        self.save()
        with self.assertRaisesRegex(ValueError, "mixed frame IDs"):
            validate(self.folder)

    def test_hand_palette_witness_must_follow_its_draw(self):
        witness = dict(kind="avatar_palette_overlap", frame=7, eye=0, draw=0,
                       status="compared", unique_matches=3)
        first_draw = next(i for i, record in enumerate(self.records) if record["kind"] == "draw")
        self.records.insert(first_draw, witness)
        self.save()
        with self.assertRaisesRegex(ValueError, "invalid hand palette witness draw identity"):
            validate(self.folder)
        witness["draw"] = 8
        self.save()
        with self.assertRaisesRegex(ValueError, "invalid hand palette witness draw identity"):
            validate(self.folder)

    def test_hand_skin_upload_precedes_the_recorded_draw(self):
        upload = dict(kind="hand_skin_upload", frame=7, eye=0, before_draw=0,
                      start=40, count=10, bank_first=45, bank_count=5,
                      bank_fnv64="1234567890abcdef", forwarded_bank_fnv64="1234567890abcdef",
                      vs_hash=123, caller="0x1234",
                      caller_exe_rva=1234, hr=0)
        first_draw = next(i for i, record in enumerate(self.records) if record["kind"] == "draw")
        self.records.insert(first_draw, upload)
        self.records.insert(-3, dict(kind="hand_skin_upload_summary", frame=7,
                                     seen=1, saved=1, truncated=False))
        self.save()
        self.assertEqual(validate(self.folder)["frame"], 7)
        upload["forwarded_bank_fnv64"] = "fedcba0987654321"
        self.save()
        self.assertEqual(validate(self.folder)["frame"], 7)
        upload["before_draw"] = 1
        self.save()
        with self.assertRaisesRegex(ValueError, "invalid hand skin upload identity"):
            validate(self.folder)

    def test_empty_hand_skin_upload_summary_is_valid(self):
        self.records.insert(-3, dict(kind="hand_skin_upload_summary", frame=7,
                                     seen=0, saved=0, truncated=False))
        self.save()
        self.assertEqual(validate(self.folder)["frame"], 7)

    def test_hand_skin_upload_incomplete_or_failed_is_rejected(self):
        upload = dict(kind="hand_skin_upload", frame=7, eye=0, before_draw=0,
                      start=45, count=3, bank_first=45, bank_count=3,
                      bank_fnv64="1234567890abcdef", forwarded_bank_fnv64="1234567890abcdef",
                      vs_hash=0, caller="0x1234",
                      caller_exe_rva=None, hr=0)
        first_draw = next(i for i, record in enumerate(self.records) if record["kind"] == "draw")
        self.records.insert(first_draw, upload)
        summary = dict(kind="hand_skin_upload_summary", frame=7, seen=1, saved=1, truncated=False)
        self.records.insert(-3, summary)
        for key, value, message in (("hr", -1, "identity"), ("bank_count", 2, "identity"),
                                    ("start", 147, "identity")):
            with self.subTest(key=key):
                old=upload[key];upload[key]=value;self.save()
                with self.assertRaisesRegex(ValueError, f"invalid hand skin upload {message}"):
                    validate(self.folder)
                upload[key]=old
        summary["seen"] = 2
        self.save()
        with self.assertRaisesRegex(ValueError, "invalid hand skin upload summary"):
            validate(self.folder)
        summary["truncated"] = True
        self.save()
        self.assertEqual(validate(self.folder)["frame"], 7)

    def test_hand_skin_upload_stops_at_last_consumed_matrix(self):
        upload = dict(kind="hand_skin_upload", frame=7, eye=0, before_draw=0,
                      start=146, count=2, bank_first=146, bank_count=1,
                      bank_fnv64="1234567890abcdef", forwarded_bank_fnv64="fedcba0987654321",
                      vs_hash=0, caller="0x1234", caller_exe_rva=None, hr=0)
        first_draw = next(i for i, record in enumerate(self.records) if record["kind"] == "draw")
        self.records.insert(first_draw, upload)
        self.records.insert(-3, dict(kind="hand_skin_upload_summary", frame=7,
                                     seen=1, saved=1, truncated=False))
        self.save()
        self.assertEqual(validate(self.folder)["frame"], 7)
        upload["bank_count"] = 2
        self.save()
        with self.assertRaisesRegex(ValueError, "invalid hand skin upload identity"):
            validate(self.folder)

    def test_no_present_completion(self):
        self.records=[r for r in self.records if r["kind"]!="present_completion"]
        self.save()
        with self.assertRaisesRegex(ValueError,"missing current Present"):
            validate(self.folder)

    def test_stale_or_failed_present(self):
        completion=next(r for r in self.records if r["kind"]=="present_completion")
        for key,value in (("epoch",2),("serial",13),("game_frame",10),("same_witness",False),
                          ("forwarded",False),("hr",0x88760868),("hr",0x08760868)):
            with self.subTest(key=key,value=value):
                original=completion[key];completion[key]=value;self.save()
                with self.assertRaisesRegex(ValueError,"invalid Present completion"):
                    validate(self.folder)
                completion[key]=original

    def test_endscene_is_not_present(self):
        next(r for r in self.records if r["kind"]=="present_entry")["boundary"]="EndScene@42(fallback)"
        self.save()
        with self.assertRaisesRegex(ValueError,"invalid Present completion"):
            validate(self.folder)

    def test_arm_to_closure_identity(self):
        arm=self.records[1]
        for key,value in (("expected_serial",11),("expected_game_frame",8),("thread",4),
                          ("reset_generation",3),("device","other-dev"),("epoch",2)):
            with self.subTest(key=key):
                original=arm[key];arm[key]=value;self.save()
                with self.assertRaisesRegex(ValueError,"arm does not match closing Present"):
                    validate(self.folder)
                arm[key]=original

    def test_completion_before_submission(self):
        completion=self.records.pop(-2);self.records.insert(2,completion);self.save()
        with self.assertRaisesRegex(ValueError,"invalid Present boundary ordering"):
            validate(self.folder)

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
        index=next(i for i,r in enumerate(self.records) if r["kind"]=="source")
        self.records.insert(index+1,dict(self.records[index]))
        self.save()
        with self.assertRaisesRegex(ValueError, "missing or duplicate"):
            validate(self.folder)

    def test_overwritten_source(self):
        index=next(i for i,r in enumerate(self.records) if r["kind"]=="source")
        source = self.records.pop(index)
        replay=next(i for i,r in enumerate(self.records) if r["kind"]=="draw" and r["eye"]==1)
        self.records.insert(replay+1, source)
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

    def test_optional_pass_stage_hash_and_payload(self):
        name = "stage-01-draw-0.bin"
        payload = bytes(512*512*8)
        (self.folder/name).write_bytes(payload)
        record = dict(kind="pass_stage",frame=7,eye=0,last_draw=0,width=512,height=512,format=113,
                      file=name,saved=True,hr=0,sha256=hashlib.sha256(payload).hexdigest())
        self.records.insert(3,record);self.save()
        self.assertEqual(validate(self.folder)["frame"],7)
        (self.folder/name).write_bytes(b"corrupt")
        with self.assertRaisesRegex(ValueError,"auxiliary hash mismatch"):
            validate(self.folder)

    def test_auxiliary_paths_and_truncation_rejected(self):
        record = dict(kind="pass_stage",frame=7,eye=0,width=512,height=512,format=113,
                      file="../outside.bin",saved=True,hr=0,sha256=hashlib.sha256(b"bad").hexdigest())
        self.records.insert(3,record);self.save()
        with self.assertRaisesRegex(ValueError,"invalid auxiliary filename"):
            validate(self.folder)
        record["file"]="stage-01-draw-0.bin"
        (self.folder/record["file"]).write_bytes(b"bad");self.save()
        with self.assertRaisesRegex(ValueError,"truncated pass stage"):
            validate(self.folder)

    def test_composite_depth_input_and_phase_identity(self):
        name = "stage-01-draw-0.bin"
        payload = struct.pack("<f", 1.0) * (512*512)
        (self.folder/name).write_bytes(payload)
        record = dict(kind="pass_stage", frame=7, eye=0, last_draw=0,
                      width=512, height=512, format=114, phase="before_draw_input",
                      sampler=2, ps_hash=0x33DE98C1, file=name, saved=True, hr=0,
                      sha256=hashlib.sha256(payload).hexdigest())
        self.records.insert(3,record); self.save()
        self.assertEqual(validate(self.folder)["frame"],7)
        record["sampler"] = 1; self.save()
        with self.assertRaisesRegex(ValueError,"invalid postprocess input identity"):
            validate(self.folder)
        record["sampler"] = 2; record["ps_hash"] = 0; self.save()
        with self.assertRaisesRegex(ValueError,"invalid postprocess input identity"):
            validate(self.folder)

    def test_failed_optional_stage_is_not_required_image_evidence(self):
        self.records.insert(3,dict(kind="pass_stage",frame=7,saved=False,hr=-1))
        self.save()
        self.assertEqual(validate(self.folder)["frame"],7)

    def add_post_inputs(self):
        rows = [dict(kind="post_input", frame=7, eye=0, draw=0, ps_hash=0x33DE98C1,
                     sampler=slot, surface_available=True, texture_hr=0, surface_hr=0, desc_hr=0,
                     texture_type=3, width=512, height=512, format=113,
                     readback_status="unsupported_format" if slot==2 else "byte_budget") for slot in (0,2,3)]
        self.records[3:3] = rows
        return rows

    def test_unsupported_depth_is_explicit_and_not_image_proof(self):
        rows = self.add_post_inputs()
        rows[1]["format"] = 0x5A544E49  # INTZ: identified, not a float/readback claim.
        self.save()
        self.assertEqual(validate(self.folder)["frame"],7)

    def test_exact_shadow_input_requires_only_sampler_zero(self):
        row=dict(kind="post_input",frame=7,eye=0,draw=0,ps_hash=0xC4D1FB10,
                 sampler=0,surface_available=True,texture_hr=0,surface_hr=0,desc_hr=0,
                 texture_type=3,width=512,height=512,format=0x5A544E49,
                 readback_status="unsupported_format")
        self.records.insert(3,row);self.save()
        self.assertEqual(validate(self.folder)["frame"],7)
        row["sampler"]=2;self.save()
        with self.assertRaisesRegex(ValueError,"incomplete postprocess input"):
            validate(self.folder)
        row["sampler"]=0;self.records.insert(4,dict(row));self.save()
        with self.assertRaisesRegex(ValueError,"incomplete postprocess input"):
            validate(self.folder)

    def test_incomplete_or_inconsistent_post_inputs_rejected(self):
        rows = self.add_post_inputs()
        self.records.remove(rows[1]); self.save()
        with self.assertRaisesRegex(ValueError,"incomplete postprocess input"):
            validate(self.folder)
        self.records.insert(4,rows[1]); rows[1]["desc_hr"]=-1; self.save()
        with self.assertRaisesRegex(ValueError,"invalid postprocess surface"):
            validate(self.folder)

    def test_missing_input_is_not_saved_image(self):
        rows = self.add_post_inputs()
        rows[1].update(surface_available=False, readback_status="unbound")
        self.save()
        self.assertEqual(validate(self.folder)["frame"],7)
        rows[1]["readback_status"]="saved"; self.save()
        with self.assertRaisesRegex(ValueError,"unavailable postprocess surface"):
            validate(self.folder)

    def test_selected_native_camera_requires_matching_frame_and_both_eyes(self):
        self.config["native_camera_cover"] = True
        payload = json.dumps(self.config).encode()
        (self.folder/"effective-config.json").write_bytes(payload)
        next(r for r in self.records if r["kind"] == "config_identity")["effective_config_sha256"] = hashlib.sha256(payload).hexdigest()
        for r in self.records:
            if r["kind"] == "source": r["native_px_py_candidate"] = [.5,.7]
        self.save()
        with self.assertRaisesRegex(ValueError, "same-frame selected"): validate(self.folder)
        camera = dict(kind="selected_native_camera",frame=7,camera_frame=7,
                      projection=[.5,0,0,0,0,.7,0,0,0,0,1,1,0,0,-.1,0],view_projection=[1]*16)
        self.records.insert(-3,camera)
        self.save();validate(self.folder)
        camera["camera_frame"] = 6
        self.save()
        with self.assertRaisesRegex(ValueError, "same-frame selected"): validate(self.folder)
        camera["camera_frame"] = 7
        next(r for r in self.records if r["kind"] == "source" and r["eye"] == 1)["native_px_py_candidate"] = [1.7,1.8]
        self.save()
        with self.assertRaisesRegex(ValueError, "GPU projection disagree"): validate(self.folder)

    def test_luna_ray_compare(self):
        command = [sys.executable,str(Path(__file__).with_name("check_matched_capture.py")),str(self.folder),
                   "--eye","0","--source-fov-tan","-.54630249",".54630249",".54630249","-.54630249",
                   "--camera-proof-note","synthetic symmetric projection"]
        result = subprocess.run(command,capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertIn("synthetic symmetric projection",result.stdout)

    def test_luna_world_depth_range_preserves_color_rays(self):
        next(r for r in self.records if r["kind"] == "source")["viewport"][4] = 0.05
        self.save()
        self.test_luna_ray_compare()

    def test_luna_rejects_partial_color_viewport_or_invalid_depth(self):
        source = next(r for r in self.records if r["kind"] == "source")
        command = [sys.executable, str(Path(__file__).with_name("check_matched_capture.py")), str(self.folder),
                   "--eye", "0", "--source-fov-tan", "-.54630249", ".54630249", ".54630249", "-.54630249",
                   "--camera-proof-note", "synthetic symmetric projection"]
        for viewport in ([1,0,8,8,0,1], [0,0,7,8,0,1], [0,0,8,8,1,0], [0,0,8,8,-.1,1],
                         [0,0,8,8,0,float('nan')]):
            with self.subTest(viewport=viewport):
                source["viewport"] = viewport
                self.save()
                result = subprocess.run(command, capture_output=True, text=True)
                self.assertEqual(result.returncode, 2)
                self.assertIn("full-viewport", result.stderr)


if __name__ == "__main__":
    unittest.main()
