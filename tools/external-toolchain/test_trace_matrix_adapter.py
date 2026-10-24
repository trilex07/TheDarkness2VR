#!/usr/bin/env python3
"""test_trace_matrix_adapter.py - Comprehensive Deterministic Tests for Matrix Trace Adapter

Deterministically validates all parser correctness gates and regression fixes:
  1. Synthetic perspective projection & frustum solving (row-major).
  2. Transposed perspective matrix normalization and consistent Px/Py calculation.
  3. Camera apex (eye) recovery from known View-Projection matrices.
  4. Separation of VS and PS constant banks (Defect P1: op=27 PS writes do not contaminate VS).
  5. Persistence of D3D9 constants across RT0 changes (Removal of false STALE_CROSS_PASS).
  6. Distinction of RT0 surface identity and viewport state for same-size targets.
  7. Conservative reset of shader bindings on unknown VS (op=19 without VS line).
  8. Conservative invalidation of shader state on state block restore (op=36 ApplyStateBlock).
  9. Marking omitted bulk constant upload payloads unknown (op=20 with count > 32).
  10. Partial upload handling (op=20 with count < 4).
  11. Layout disambiguation flagging ambiguous transpose on symmetric matrices.
  12. Downgrading unproven camera identity to PROJECTION_CONSISTENT_WVP (never VALIDATED_CAMERA_WVP).
  13. Distinction between true skeletal bone palettes (start >= 32) and bulk constant uploads (start < 32).
  14. D3DFMT enum correctness: format 23 is R5G6B5, format 114 is R32F.
  15. RT slot isolation: MRT attachments do not override RT0; unbinding RT0 resets pass to UNKNOWN.
  16. Multi-pass deterministic synthetic trace fixture.
  17. Real Darkness II trace regression with explicit SKIPPED handling when trace is absent.
"""

from __future__ import annotations

import math
import os
import sys
import tempfile
import unittest
from pathlib import Path

# Ensure tools/external-toolchain is in sys.path
sys.path.insert(0, str(Path(__file__).resolve().parent))

from trace_matrix_adapter import (
    TraceAnalyzer,
    EvidenceLevel,
    RenderPassType,
    solve_camera_frustum,
    extract_gribb_hartmann_planes_d3d,
    is_rigid_affine,
    is_identity_4x4,
    is_ortho,
    transpose16,
    format_name,
)

def make_d3d_perspective_lh(fov_y_deg: float, aspect: float, zn: float, zf: float) -> list[float]:
    """Generates standard Direct3D 9 row-major perspective projection matrix."""
    fov_y_rad = math.radians(fov_y_deg)
    h = 1.0 / math.tan(fov_y_rad / 2.0)
    w = h / aspect
    q = zf / (zf - zn)
    return [
        w,   0.0, 0.0,       0.0,
        0.0, h,   0.0,       0.0,
        0.0, 0.0, q,         1.0,
        0.0, 0.0, -zn * q,   0.0,
    ]

def make_d3d_lookat_lh(eye: tuple[float, float, float], at: tuple[float, float, float], up: tuple[float, float, float] = (0.0, 1.0, 0.0)) -> list[float]:
    """Generates standard Direct3D 9 row-major view matrix."""
    def sub(a, b): return (a[0]-b[0], a[1]-b[1], a[2]-b[2])
    def dot(a, b): return a[0]*b[0] + a[1]*b[1] + a[2]*b[2]
    def norm(v):
        l = math.sqrt(dot(v, v))
        return (v[0]/l, v[1]/l, v[2]/l)
    def cross(a, b):
        return (a[1]*b[2] - a[2]*b[1], a[2]*b[0] - a[0]*b[2], a[0]*b[1] - a[1]*b[0])

    z = norm(sub(at, eye))
    x = norm(cross(up, z))
    y = cross(z, x)
    return [
        x[0], y[0], z[0], 0.0,
        x[1], y[1], z[1], 0.0,
        x[2], y[2], z[2], 0.0,
        -dot(x, eye), -dot(y, eye), -dot(z, eye), 1.0
    ]

def mul_4x4(a: list[float], b: list[float]) -> list[float]:
    """Multiplies two 4x4 row-major matrices: out = A * B."""
    out = [0.0] * 16
    for r in range(4):
        for c in range(4):
            out[r * 4 + c] = sum(a[r * 4 + k] * b[k * 4 + c] for k in range(4))
    return out

IDENTITY_4X4 = [
    1.0, 0.0, 0.0, 0.0,
    0.0, 1.0, 0.0, 0.0,
    0.0, 0.0, 1.0, 0.0,
    0.0, 0.0, 0.0, 1.0,
]

TRANSLATION_4X4 = [
    1.0, 0.0, 0.0, 0.0,
    0.0, 1.0, 0.0, 0.0,
    0.0, 0.0, 1.0, 0.0,
    15.0, -5.0, 20.0, 1.0,
]

class TestTraceMatrixAdapter(unittest.TestCase):


    def _acceptance_fixture(self, suffix):
        proj = make_d3d_perspective_lh(60.0, 1.0, 0.5, 200.0)
        rows = "\n".join(f" c{i} {' '.join(map(str, proj[4*i:4*i+4]))}" for i in range(4))
        trace = f"""0 op=1 a=0,4096,0,0,0
 RT slot=0 size=3076x3140 format=113
1 op=19 a=1000,0,0,0,0
 VS TEST guessed=0,-1 named=-1,-1
2 op=26 a=2000,0,0,0,0
 PS inverse=0 hash=TESTPS
3 op=20 a=0,4,0,0,0
{rows}
4 op=27 a=0,4,0,0,0
{rows}
5 op=14 a=4,0,0,10,0
{suffix}
"""
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)/"trace.txt"
            path.write_text(trace, encoding="utf-8")
            analyzer = TraceAnalyzer()
            analyzer.parse_file(path)
        return analyzer

    def test_viewport_opaque_offset_becomes_unknown(self):
        for offset in (0, 24, 4096):
            with self.subTest(offset=offset):
                a = self._acceptance_fixture(f"10 op=7 a={offset},0,0,0,0\n11 op=14 a=4,0,0,10,0")
                self.assertEqual(a.classifications[0].viewport, (0,0,3076,3140))
                self.assertIsNone(a.rt0_viewport)
                self.assertIsNone(a.classifications[-1].viewport)
                self.assertEqual(a.rt0_width, 3076)  # attachment dimensions remain known

    def test_viewport_unknown_is_visible_in_summary(self):
        import contextlib
        import io
        a = self._acceptance_fixture("10 op=7 a=1024,0,0,0,0\n11 op=14 a=4,0,0,10,0")
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            a.print_summary()
        self.assertIn("viewport=UNKNOWN", out.getvalue())

    def test_viewport_unknown_survives_mrt_attachment(self):
        a = self._acceptance_fixture("""10 op=7 a=128,0,0,0,0
11 op=1 a=1,8192,0,0,0
 RT slot=1 size=1538x1570 format=113
12 op=14 a=4,0,0,10,0""")
        self.assertIsNone(a.rt0_viewport)
        self.assertIsNone(a.classifications[-1].viewport)
        self.assertEqual(a.rt0_surface_ptr,4096)

    def test_rt0_bind_recovers_default_viewport_after_opaque_update(self):
        a = self._acceptance_fixture("""10 op=7 a=128,0,0,0,0
11 op=1 a=0,8192,0,0,0
 RT slot=0 size=3076x3140 format=113
12 op=14 a=4,0,0,10,0""")
        self.assertEqual(a.classifications[-1].viewport,(0,0,3076,3140))

    def test_stateblock_rebind_does_not_resurrect_vs_or_ps_constants(self):
        a = self._acceptance_fixture("""10 op=36 a=9000,0,0,0,0
11 op=19 a=1000,0,0,0,0
 VS TEST guessed=0,-1 named=-1,-1
12 op=26 a=2000,0,0,0,0
 PS inverse=0 hash=TESTPS
13 op=14 a=4,0,0,10,0""")
        self.assertEqual(len(a.classifications),1)
        for bank in (a.vs_constants,a.ps_constants,a.vs_upload_cmd,a.ps_upload_cmd,a.vs_upload_pass,a.ps_upload_pass):
            self.assertEqual(bank,{})
        self.assertIsNone(a.rt0_viewport)

    def test_stateblock_partial_upload_cannot_mix_old_matrix_rows(self):
        a = self._acceptance_fixture("""10 op=36 a=9000,0,0,0,0
11 op=19 a=1000,0,0,0,0
 VS TEST guessed=0,-1 named=-1,-1
12 op=20 a=0,2,0,0,0
 c0 1 0 0 0
 c1 0 1 0 0
13 op=27 a=2,2,0,0,0
 c2 0 0 1 0
 c3 0 0 0 1
14 op=14 a=4,0,0,10,0""")
        self.assertEqual(set(a.vs_constants),{0,1})
        self.assertEqual(set(a.ps_constants),{2,3})
        self.assertEqual(len(a.classifications),1)

    def test_stateblock_fresh_banks_recover_independently(self):
        proj = make_d3d_perspective_lh(90.0,1.5,0.5,200.0)
        rows = "\n".join(f" c{i} {' '.join(map(str, proj[4*i:4*i+4]))}" for i in range(4))
        a = self._acceptance_fixture(f"""10 op=36 a=9000,0,0,0,0
11 op=19 a=1000,0,0,0,0
 VS TEST guessed=0,-1 named=-1,-1
12 op=20 a=0,4,0,0,0
{rows}
13 op=27 a=0,1,0,0,0
 c0 -9 -9 -9 -9
14 op=14 a=4,0,0,10,0""")
        self.assertEqual(len(a.classifications),2)
        self.assertAlmostEqual(a.classifications[-1].fov_y_deg,90,delta=.01)
        self.assertEqual(a.vs_upload_cmd,{i:12 for i in range(4)})
        self.assertEqual(a.ps_upload_cmd,{0:13})
        self.assertEqual(a.ps_constants,{0:[-9.0]*4})

    def test_stateblock_clears_palette_and_orphan_upload_context(self):
        a = self._acceptance_fixture("""10 op=20 a=45,20,0,0,0
 c45 1 2 3 4
11 op=36 a=9000,0,0,0,0
 c46 5 6 7 8""")
        self.assertEqual(a.bone_palette_registers,set())
        self.assertEqual(a.vs_constants,{})
        self.assertIsNone(a.active_upload_target)
        self.assertEqual(a.active_upload_cmd,-1)
        self.assertEqual(a.active_upload_start,-1)
        self.assertEqual(a.active_upload_count,-1)

    def test_synthetic_camera_projection(self):
        """1. Validates frustum plane extraction on pure 60-degree D3D9 perspective matrix."""
        proj = make_d3d_perspective_lh(fov_y_deg=60.0, aspect=1.5, zn=0.1, zf=100.0)
        res = solve_camera_frustum(proj)
        self.assertIsNotNone(res)
        fov_x, fov_y, aspect, eye, vz = res
        self.assertAlmostEqual(fov_y, 60.0, places=2)
        self.assertAlmostEqual(aspect, 1.5, places=2)
        for val in eye:
            self.assertAlmostEqual(val, 0.0, places=2)

    def test_transposed_perspective_coefficients(self):
        """2. Validates that transposed (column-major) storage layout is normalized consistently to row-vector convention."""
        proj = make_d3d_perspective_lh(fov_y_deg=60.0, aspect=1.0, zn=0.1, zf=100.0)
        proj_t = transpose16(proj)

        res = solve_camera_frustum(proj_t)
        self.assertIsNone(res, "Raw transposed matrix should not solve directly under row-major assumptions")

        # After transpose normalization
        norm = transpose16(proj_t)
        res_norm = solve_camera_frustum(norm)
        self.assertIsNotNone(res_norm)
        col0 = (norm[0], norm[4], norm[8])
        col1 = (norm[1], norm[5], norm[9])
        col3 = (norm[3], norm[7], norm[11])
        px = math.sqrt(sum(x*x for x in col0)) / math.sqrt(sum(x*x for x in col3))
        py = math.sqrt(sum(x*x for x in col1)) / math.sqrt(sum(x*x for x in col3))
        self.assertAlmostEqual(py, 1.73205, places=3)
        self.assertAlmostEqual(px, 1.73205, places=3)

    def test_synthetic_view_projection_apex(self):
        """3. Validates recovery of known camera eye from View-Projection matrix."""
        target_eye = (10.0, 20.0, 30.0)
        view = make_d3d_lookat_lh(eye=target_eye, at=(10.0, 20.0, 100.0))
        proj = make_d3d_perspective_lh(fov_y_deg=60.0, aspect=1.0, zn=0.5, zf=200.0)
        vp = mul_4x4(view, proj)

        res = solve_camera_frustum(vp)
        self.assertIsNotNone(res)
        fov_x, fov_y, aspect, eye, vz = res
        self.assertAlmostEqual(fov_y, 60.0, places=2)
        self.assertAlmostEqual(eye[0], target_eye[0], places=2)
        self.assertAlmostEqual(eye[1], target_eye[1], places=2)
        self.assertAlmostEqual(eye[2], target_eye[2], places=2)

    def test_vs_ps_constant_bank_separation(self):
        """4. Defect P1: Ensures VS and PS constant banks are strictly separated.
        Uploading PS c0=(9,8,7,6) via op=27 must not overwrite VS c0=(1,2,3,4) from op=20."""
        trace_text = """
0 op=1 a=0,100,0,0,0
 RT slot=0 size=3076x3140 format=113
1 op=19 a=1000,0,0,0,0
 VS SEPARATE_TEST guessed=0,-1 named=-1,-1
2 op=20 a=0,4,0,0,10
 c0 1.0 2.0 3.0 4.0
 c1 0.0 1.0 0.0 0.0
 c2 0.0 0.0 1.0 0.0
 c3 0.0 0.0 0.0 1.0
3 op=27 a=0,4,0,0,20
 c0 9.0 8.0 7.0 6.0
 c1 5.0 5.0 5.0 5.0
 c2 5.0 5.0 5.0 5.0
 c3 5.0 5.0 5.0 5.0
4 op=14 a=4,0,0,30,0
"""
        with tempfile.NamedTemporaryFile("w", delete=False) as tf:
            tf.write(trace_text)
            temp_path = tf.name

        try:
            analyzer = TraceAnalyzer()
            analyzer.parse_file(temp_path)
            # VS bank must retain (1, 2, 3, 4)
            self.assertEqual(analyzer.vs_constants[0], [1.0, 2.0, 3.0, 4.0])
            self.assertEqual(analyzer.vs_upload_cmd[0], 2)

            # PS bank must retain (9, 8, 7, 6)
            self.assertEqual(analyzer.ps_constants[0], [9.0, 8.0, 7.0, 6.0])
            self.assertEqual(analyzer.ps_upload_cmd[0], 3)
        finally:
            os.remove(temp_path)

    def test_persistent_vs_constants_across_rt0_changes(self):
        """5. Defect P1: D3D9 constants persist across SetRenderTarget calls.
        Constants uploaded in an earlier pass remain valid and are not falsely rejected as STALE_CROSS_PASS."""
        proj = make_d3d_perspective_lh(fov_y_deg=60.0, aspect=1.0, zn=0.5, zf=200.0)
        c0_lines = "\n".join(f" c{i} {' '.join(str(proj[i*4 + j]) for j in range(4))}" for i in range(4))

        trace_text = f"""
0 op=1 a=0,100,0,0,0
 RT slot=0 size=4096x4096 format=23
1 op=19 a=1000,0,0,0,0
 VS SHADOW guessed=0,-1 named=-1,-1
2 op=20 a=0,4,0,0,10
{c0_lines}
3 op=14 a=4,0,0,10,0
4 op=1 a=0,200,0,0,0
 RT slot=0 size=3076x3140 format=113
5 op=19 a=2000,0,0,0,0
 VS SCENE guessed=0,-1 named=-1,-1
6 op=14 a=4,0,0,20,0
"""
        with tempfile.NamedTemporaryFile("w", delete=False) as tf:
            tf.write(trace_text)
            temp_path = tf.name

        try:
            analyzer = TraceAnalyzer()
            analyzer.parse_file(temp_path)
            # Both draws must be evaluated without STALE_CROSS_PASS rejection
            self.assertEqual(len(analyzer.classifications), 2)
            c1 = analyzer.classifications[0]
            c2 = analyzer.classifications[1]

            self.assertEqual(c1.evidence, EvidenceLevel.SHADOW_PASS_WVP)
            self.assertEqual(c1.pass_type, RenderPassType.SHADOW_ATLAS)

            self.assertEqual(c2.evidence, EvidenceLevel.PROJECTION_CONSISTENT_WVP)
            self.assertEqual(c2.pass_type, RenderPassType.MAIN_HDR_SCENE)
            self.assertEqual(c2.upload_command, 2)
            self.assertEqual(c2.upload_pass, RenderPassType.SHADOW_ATLAS)
            self.assertIn("Uploaded at cmd 2 in SHADOW_ATLAS", c2.notes[0])
            self.assertIn("drawn at cmd 6 in MAIN_HDR_SCENE", c2.notes[0])
        finally:
            os.remove(temp_path)

    def test_same_size_different_rt0_identities(self):
        """6. Defect P1: RT identity is tracked by surface pointer, not merely dimensions.
        Two 3076x3140 targets with different surface pointers are distinguished."""
        proj = make_d3d_perspective_lh(fov_y_deg=60.0, aspect=1.0, zn=0.5, zf=200.0)
        c0_lines = "\n".join(f" c{i} {' '.join(str(proj[i*4 + j]) for j in range(4))}" for i in range(4))

        trace_text = f"""
0 op=1 a=0,4096,0,0,0
 RT slot=0 size=3076x3140 format=113
1 op=19 a=1000,0,0,0,0
 VS TEST1 guessed=0,-1 named=-1,-1
2 op=20 a=0,4,0,0,10
{c0_lines}
3 op=14 a=4,0,0,10,0
4 op=1 a=0,8192,0,0,0
 RT slot=0 size=3076x3140 format=113
5 op=19 a=1000,0,0,0,0
 VS TEST1 guessed=0,-1 named=-1,-1
6 op=14 a=4,0,0,20,0
"""
        with tempfile.NamedTemporaryFile("w", delete=False) as tf:
            tf.write(trace_text)
            temp_path = tf.name

        try:
            analyzer = TraceAnalyzer()
            analyzer.parse_file(temp_path)
            self.assertEqual(len(analyzer.classifications), 2)
            # Draw 3 was on surface 4096
            self.assertEqual(analyzer.classifications[0].rt0_identity, 4096)
            self.assertEqual(analyzer.classifications[0].viewport, (0, 0, 3076, 3140))
            # Draw 6 was on surface 8192
            self.assertEqual(analyzer.classifications[1].rt0_identity, 8192)
            self.assertEqual(analyzer.classifications[1].viewport, (0, 0, 3076, 3140))
        finally:
            os.remove(temp_path)

    def test_unknown_vs_binding_reset(self):
        """7. Defect P1: op=19 without subsequent VS line conservatively resets active VS bindings."""
        proj = make_d3d_perspective_lh(fov_y_deg=60.0, aspect=1.0, zn=0.5, zf=200.0)
        c0_lines = "\n".join(f" c{i} {' '.join(str(proj[i*4 + j]) for j in range(4))}" for i in range(4))

        trace_text = f"""
0 op=1 a=0,100,0,0,0
 RT slot=0 size=3076x3140 format=113
1 op=19 a=1000,0,0,0,0
 VS BOUND_VS guessed=0,-1 named=-1,-1
2 op=20 a=0,4,0,0,10
{c0_lines}
3 op=14 a=4,0,0,10,0
4 op=19 a=0,0,0,0,0
5 op=14 a=4,0,0,20,0
"""
        with tempfile.NamedTemporaryFile("w", delete=False) as tf:
            tf.write(trace_text)
            temp_path = tf.name

        try:
            analyzer = TraceAnalyzer()
            analyzer.parse_file(temp_path)
            # Only Draw 3 should produce a classification; Draw 5 has unknown VS binding
            self.assertEqual(len(analyzer.classifications), 1)
            self.assertEqual(analyzer.classifications[0].draw_command, 3)
            self.assertEqual(analyzer.active_vs_hash, "NONE")
            self.assertEqual(analyzer.active_vs_guessed_wvp, -1)
        finally:
            os.remove(temp_path)

    def test_state_block_restore(self):
        """8. Defect P1: op=36 (ApplyStateBlock) conservatively clears active shader bindings."""
        proj = make_d3d_perspective_lh(fov_y_deg=60.0, aspect=1.0, zn=0.5, zf=200.0)
        c0_lines = "\n".join(f" c{i} {' '.join(str(proj[i*4 + j]) for j in range(4))}" for i in range(4))

        trace_text = f"""
0 op=1 a=0,100,0,0,0
 RT slot=0 size=3076x3140 format=113
1 op=19 a=1000,0,0,0,0
 VS BOUND_VS guessed=0,-1 named=-1,-1
2 op=20 a=0,4,0,0,10
{c0_lines}
3 op=36 a=50,0,0,0,0
4 op=14 a=4,0,0,20,0
"""
        with tempfile.NamedTemporaryFile("w", delete=False) as tf:
            tf.write(trace_text)
            temp_path = tf.name

        try:
            analyzer = TraceAnalyzer()
            analyzer.parse_file(temp_path)
            self.assertTrue(analyzer.state_block_restored)
            self.assertEqual(analyzer.active_vs_hash, "NONE")
            self.assertEqual(analyzer.active_vs_guessed_wvp, -1)
            # Draw 4 occurs after state block restore without re-bound shader; no classification emitted
            self.assertEqual(len(analyzer.classifications), 0)
        finally:
            os.remove(temp_path)

    def test_missing_bulk_payload(self):
        """9. Defect P1: Omitted bulk constant upload (count > 32 where trace omits cN lines)
        marks registers unknown; does not solve frustum from missing data."""
        trace_text = """
0 op=1 a=0,100,0,0,0
 RT slot=0 size=3076x3140 format=113
1 op=19 a=1000,0,0,0,0
 VS BULK_VS guessed=0,-1 named=-1,-1
2 op=20 a=0,40,0,0,10
3 op=14 a=4,0,0,20,0
"""
        with tempfile.NamedTemporaryFile("w", delete=False) as tf:
            tf.write(trace_text)
            temp_path = tf.name

        try:
            analyzer = TraceAnalyzer()
            analyzer.parse_file(temp_path)
            # Registers 0..39 were overwritten by op=20 with omitted payload, so vs_constants is empty
            self.assertNotIn(0, analyzer.vs_constants)
            self.assertEqual(len(analyzer.classifications), 0)
        finally:
            os.remove(temp_path)

    def test_partial_uploads(self):
        """10. Defect P1: Partial constant uploads (fewer than 4 registers) are not evaluated as 4x4 matrices."""
        trace_text = """
0 op=1 a=0,100,0,0,0
 RT slot=0 size=3076x3140 format=113
1 op=19 a=1000,0,0,0,0
 VS PARTIAL_VS guessed=0,-1 named=-1,-1
2 op=20 a=0,2,0,0,10
 c0 1.0 0.0 0.0 0.0
 c1 0.0 1.0 0.0 0.0
3 op=14 a=4,0,0,20,0
"""
        with tempfile.NamedTemporaryFile("w", delete=False) as tf:
            tf.write(trace_text)
            temp_path = tf.name

        try:
            analyzer = TraceAnalyzer()
            analyzer.parse_file(temp_path)
            self.assertIn(0, analyzer.vs_constants)
            self.assertIn(1, analyzer.vs_constants)
            self.assertNotIn(2, analyzer.vs_constants)
            self.assertNotIn(3, analyzer.vs_constants)
            self.assertEqual(len(analyzer.classifications), 0)
        finally:
            os.remove(temp_path)

    def test_ambiguous_transpose(self):
        """11. Defect P2: Flags layout as ambiguous when both row-major and transposed forms solve."""
        # A symmetric perspective projection matrix (M = M^T)
        m_sym = [
            1.732, 0.0,   0.0,   0.0,
            0.0,   1.732, 0.0,   0.0,
            0.0,   0.0,   1.0,   1.0,
            0.0,   0.0,   1.0,   0.0,
        ]
        c0_lines = "\n".join(f" c{i} {' '.join(str(m_sym[i*4 + j]) for j in range(4))}" for i in range(4))

        trace_text = f"""
0 op=1 a=0,100,0,0,0
 RT slot=0 size=3076x3140 format=113
1 op=19 a=1000,0,0,0,0
 VS SYM_VS guessed=0,-1 named=-1,-1
2 op=20 a=0,4,0,0,10
{c0_lines}
3 op=14 a=4,0,0,10,0
"""
        with tempfile.NamedTemporaryFile("w", delete=False) as tf:
            tf.write(trace_text)
            temp_path = tf.name

        try:
            analyzer = TraceAnalyzer()
            analyzer.parse_file(temp_path)
            self.assertEqual(len(analyzer.classifications), 1)
            # Layout must be flagged as ambiguous
            self.assertEqual(analyzer.classifications[0].layout, "ambiguous")
        finally:
            os.remove(temp_path)

    def test_auxiliary_camera_downgraded_to_projection_consistency(self):
        """12. Defect P2: Matrices with W=Identity and matching Px/Py are classified as
        PROJECTION_CONSISTENT_WVP, NEVER VALIDATED_CAMERA_WVP."""
        cam_proj = [
            1.768088, 0.0,      0.0,       0.0,
            0.0,      1.732051, 0.0,       0.0,
            0.0,      0.0,      1.0001,    1.0,
            0.0,      0.0,      -0.09501,  0.0,
        ]
        c0_cam = "\n".join(f" c{i} {' '.join(str(cam_proj[i*4 + j]) for j in range(4))}" for i in range(4))
        c4_ident = "\n".join(f" c{4+i} {' '.join(str(IDENTITY_4X4[i*4 + j]) for j in range(4))}" for i in range(4))

        trace_text = f"""
0 op=1 a=0,100,0,0,0
 RT slot=0 size=3076x3140 format=113
1 op=19 a=1000,0,0,0,0
 VS TEST guessed=0,-1 named=0,-1
2 op=20 a=0,8,0,0,10
{c0_cam}
{c4_ident}
3 op=14 a=4,0,0,10,0
"""
        with tempfile.NamedTemporaryFile("w", delete=False) as tf:
            tf.write(trace_text)
            temp_path = tf.name

        try:
            analyzer = TraceAnalyzer()
            analyzer.parse_file(temp_path)
            self.assertEqual(len(analyzer.classifications), 1)
            c = analyzer.classifications[0]
            # Must be PROJECTION_CONSISTENT_WVP
            self.assertEqual(c.evidence, EvidenceLevel.PROJECTION_CONSISTENT_WVP)
            self.assertIn("auxiliary camera candidate", c.notes[0])
            self.assertIn("World=Identity verified", c.notes[0])
        finally:
            os.remove(temp_path)

    def test_true_bone_palette_versus_bulk_wvp(self):
        """13. Defect P2: Bulk upload at c0 (start < 32, count=20) is NOT quarantined as a bone palette,
        whereas upload at c45 (start >= 32, count=165) IS quarantined as BONE_PALETTE_BLOCK."""
        cam_proj = [
            1.768088, 0.0,      0.0,       0.0,
            0.0,      1.732051, 0.0,       0.0,
            0.0,      0.0,      1.0001,    1.0,
            0.0,      0.0,      -0.09501,  0.0,
        ]
        c0_cam = "\n".join(f" c{i} {' '.join(str(cam_proj[i*4 + j]) for j in range(4))}" for i in range(4))
        c4_ident = "\n".join(f" c{4+i} {' '.join(str(IDENTITY_4X4[i*4 + j]) for j in range(4))}" for i in range(4))

        trace_text = f"""
0 op=1 a=0,100,0,0,0
 RT slot=0 size=3076x3140 format=113
1 op=19 a=1000,0,0,0,0
 VS BULK_TEST guessed=0,-1 named=0,-1
2 op=20 a=0,20,0,0,10
{c0_cam}
{c4_ident}
3 op=14 a=4,0,0,10,0
4 op=19 a=2000,0,0,0,0
 VS BONE_TEST guessed=45,-1 named=-1,-1
5 op=20 a=45,165,0,0,20
6 op=14 a=4,0,0,20,0
"""
        with tempfile.NamedTemporaryFile("w", delete=False) as tf:
            tf.write(trace_text)
            temp_path = tf.name

        try:
            analyzer = TraceAnalyzer()
            analyzer.parse_file(temp_path)
            self.assertEqual(len(analyzer.classifications), 2)
            # Draw 3 (c0..c3) is evaluated as WVP
            self.assertEqual(analyzer.classifications[0].evidence, EvidenceLevel.PROJECTION_CONSISTENT_WVP)
            # Draw 6 (c45) is quarantined as bone palette
            self.assertEqual(analyzer.classifications[1].evidence, EvidenceLevel.BONE_PALETTE_BLOCK)
            self.assertIn("Quarantined", analyzer.classifications[1].notes[0])
        finally:
            os.remove(temp_path)

    def test_d3dfmt_constants_correctness(self):
        """14. Defect P2: Format 23 is D3DFMT_R5G6B5 (NOT R32F); format 114 is D3DFMT_R32F."""
        self.assertEqual(format_name(50), "L8")
        self.assertEqual(format_name(80), "D16")
        self.assertEqual(format_name(23), "R5G6B5")
        self.assertEqual(format_name(114), "R32F")
        self.assertEqual(format_name(113), "A16B16G16R16F")

        proj = make_d3d_perspective_lh(fov_y_deg=60.0, aspect=1.0, zn=0.5, zf=200.0)
        c0_lines = "\n".join(f" c{i} {' '.join(str(proj[i*4 + j]) for j in range(4))}" for i in range(4))

        trace_text = f"""
0 op=1 a=0,100,0,0,0
 RT slot=0 size=4096x4096 format=23
1 op=19 a=1000,0,0,0,0
 VS SHADOW guessed=0,-1 named=-1,-1
2 op=20 a=0,4,0,0,10
{c0_lines}
3 op=14 a=4,0,0,10,0
"""
        with tempfile.NamedTemporaryFile("w", delete=False) as tf:
            tf.write(trace_text)
            temp_path = tf.name

        try:
            analyzer = TraceAnalyzer()
            analyzer.parse_file(temp_path)
            self.assertEqual(len(analyzer.classifications), 1)
            c = analyzer.classifications[0]
            self.assertEqual(c.evidence, EvidenceLevel.SHADOW_PASS_WVP)
            # Must report R5G6B5, never R32F
            self.assertIn("4096x4096 R5G6B5 atlas", c.notes[0])
            self.assertNotIn("R32F", c.notes[0])
        finally:
            os.remove(temp_path)

    def test_rt_switching_and_mrt_isolation(self):
        """15. Validates that pass determination is governed strictly by RT0, not auxiliary MRTs,
        and unbinding RT0 resets pass to UNKNOWN_PASS."""
        analyzer = TraceAnalyzer()
        # Set RT0
        analyzer.rt_slots[0] = (3076, 3140, 113, 100)
        analyzer.update_pass_type()
        self.assertEqual(analyzer.current_pass, RenderPassType.MAIN_HDR_SCENE)

        # Set MRT slot 1 to 4096x4096 - RT0 must NOT be overridden
        analyzer.rt_slots[1] = (4096, 4096, 23, 200)
        analyzer.update_pass_type()
        self.assertEqual(analyzer.current_pass, RenderPassType.MAIN_HDR_SCENE)

        # Unbind RT0
        analyzer.rt_slots.pop(0, None)
        analyzer.update_pass_type()
        self.assertEqual(analyzer.current_pass, RenderPassType.UNKNOWN_PASS)

    def test_deterministic_synthetic_trace_fixture(self):
        """16. Runs a complete multi-pass synthetic trace fixture verifying all pipeline features."""
        cam_proj = [
            1.768088, 0.0,      0.0,       0.0,
            0.0,      1.732051, 0.0,       0.0,
            0.0,      0.0,      1.0001,    1.0,
            0.0,      0.0,      -0.09501,  0.0,
        ]
        c0_lines = "\n".join(f" c{i} {' '.join(str(cam_proj[i*4 + j]) for j in range(4))}" for i in range(4))
        c4_lines = "\n".join(f" c{4+i} {' '.join(str(IDENTITY_4X4[i*4 + j]) for j in range(4))}" for i in range(4))

        fixture = f"""
0 op=1 a=0,100,0,0,0
 RT slot=0 size=4096x4096 format=23
1 op=19 a=1000,0,0,0,0
 VS SHADOW_VS guessed=0,-1 named=-1,-1
2 op=20 a=0,4,0,0,10
{c0_lines}
3 op=14 a=4,0,0,10,0
4 op=1 a=0,200,0,0,0
 RT slot=0 size=3076x3140 format=113
5 op=19 a=2000,0,0,0,0
 VS SCENE_VS guessed=0,4 named=0,4
6 op=20 a=0,8,0,0,20
{c0_lines}
{c4_lines}
7 op=14 a=4,0,0,20,0
"""
        with tempfile.NamedTemporaryFile("w", delete=False) as tf:
            tf.write(fixture)
            temp_path = tf.name

        try:
            analyzer = TraceAnalyzer()
            analyzer.parse_file(temp_path)
            evidences = [c.evidence for c in analyzer.classifications]
            self.assertIn(EvidenceLevel.SHADOW_PASS_WVP, evidences)
            self.assertIn(EvidenceLevel.PROJECTION_CONSISTENT_WVP, evidences)
            self.assertIn(EvidenceLevel.SCREEN_TEXTURE_PROJECTION, evidences)
        finally:
            os.remove(temp_path)

    def test_real_darkness2_trace_reconciliation(self):
        """17. Verifies regression against real Darkness II frame trace run_43052_frame_003_trace.txt.
        Explicitly reports SKIPPED when the private local trace is unavailable."""
        real_trace = Path("C:/Users/Trilex/Darkness2VR/test-output/run_43052_frame_003_trace.txt")
        if not real_trace.is_file():
            self.skipTest("Real trace file not found at " + str(real_trace))

        analyzer = TraceAnalyzer()
        analyzer.parse_file(real_trace)

        # 1. Bone palette registers must be quarantined
        self.assertIn(45, analyzer.bone_palette_registers)
        self.assertIn(209, analyzer.bone_palette_registers)

        # 2. Camera projection consistent WVP must be detected with 60 deg vertical FOV
        consistent_cameras = [c for c in analyzer.classifications if c.evidence == EvidenceLevel.PROJECTION_CONSISTENT_WVP]
        self.assertGreater(len(consistent_cameras), 0)
        for cam in consistent_cameras:
            self.assertAlmostEqual(cam.fov_y_deg, 60.0, delta=1.5)
            self.assertAlmostEqual(cam.fov_x_deg, 58.98, delta=1.5)

        # 3. Shadow passes must be distinguished
        shadow_wvps = [c for c in analyzer.classifications if c.evidence == EvidenceLevel.SHADOW_PASS_WVP]
        self.assertGreater(len(shadow_wvps), 0)
        self.assertEqual(shadow_wvps[0].pass_type, RenderPassType.SHADOW_ATLAS)
        self.assertIn("R5G6B5", shadow_wvps[0].notes[0])

if __name__ == "__main__":
    loader = unittest.TestLoader()
    suite = loader.loadTestsFromTestCase(TestTraceMatrixAdapter)
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)

    total = result.testsRun
    failed = len(result.failures) + len(result.errors)
    skipped = len(result.skipped)
    passed = total - failed - skipped

    print("=" * 70)
    print(f"ADAPTER TEST RESULTS: Total={total} | Passed={passed} | Failed={failed} | Skipped={skipped}")
    print("=" * 70)

    sys.exit(0 if result.wasSuccessful() else 1)
