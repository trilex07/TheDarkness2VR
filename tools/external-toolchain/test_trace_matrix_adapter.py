#!/usr/bin/env python3
"""test_trace_matrix_adapter.py - Comprehensive Deterministic Tests for Matrix Trace Adapter

Tests:
  1. Synthetic perspective projection & frustum solving (row-major).
  2. Transposed perspective matrix normalization and consistent Px/Py calculation.
  3. Camera apex (eye) recovery from known View-Projection matrices.
  4. Negative test: Skeletal bone palette uploads (>16 constants) are quarantined.
  5. Negative test: Active shader designating a bone palette register (c45) as WVP
     is quarantined as BONE_PALETTE_BLOCK and never classified as camera WVP.
  6. Negative test: Stale cross-pass constants (uploaded in an earlier pass) are flagged
     as STALE_CROSS_PASS, while legitimate intra-pass constants remain valid across draws.
  7. Render target slot tracking: MRT slot attachments do not override RT0 pass determination;
     RT0 unbinding resets pass to UNKNOWN_PASS.
  8. Distinction between VALIDATED_CAMERA_WVP (World=Identity verified),
     PROJECTION_CONSISTENT_WVP (Px/Py match but W=I unverified), and
     CORRELATED_OBJECT_WVP (non-identity model transform W present).
  9. Deterministic synthetic trace fixture running self-contained on any machine.
  10. Real Darkness II trace regression with explicit SKIPPED handling when unavailable.
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

    def test_synthetic_camera_projection(self):
        """Validates frustum plane extraction on pure 60-degree D3D9 perspective matrix."""
        proj = make_d3d_perspective_lh(fov_y_deg=60.0, aspect=1.5, zn=0.1, zf=100.0)
        res = solve_camera_frustum(proj)
        self.assertIsNotNone(res)
        fov_x, fov_y, aspect, eye, vz = res
        self.assertAlmostEqual(fov_y, 60.0, places=2)
        self.assertAlmostEqual(aspect, 1.5, places=2)
        for val in eye:
            self.assertAlmostEqual(val, 0.0, places=2)

    def test_transposed_perspective_coefficients(self):
        """Validates that transposed (column-major) storage layout is normalized consistently to row-vector convention."""
        proj = make_d3d_perspective_lh(fov_y_deg=60.0, aspect=1.0, zn=0.1, zf=100.0)
        # Expected: Py = cot(30) = sqrt(3) ~= 1.73205, Px = 1.73205
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
        """Validates recovery of known camera eye from View-Projection matrix."""
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

    def test_negative_bone_palette_exclusion(self):
        """Ensures that a 165-register bone palette upload is tracked and quarantined."""
        trace_text = """
0 op=1 a=0,0,0,0,0
 RT slot=0 size=3076x3140 format=113
1 op=19 a=1000,0,0,0,0
 VS AABBCCDD guessed=-1,-1 named=-1,-1
2 op=20 a=45,165,0,0,100
3 op=14 a=4,0,0,100,0
"""
        with tempfile.NamedTemporaryFile("w", delete=False) as tf:
            tf.write(trace_text)
            temp_path = tf.name

        try:
            analyzer = TraceAnalyzer()
            analyzer.parse_file(temp_path)
            self.assertEqual(len(analyzer.bone_palette_registers), 165)
            self.assertEqual(len(analyzer.classifications), 0)
        finally:
            os.remove(temp_path)

    def test_negative_bone_palette_designated_as_wvp(self):
        """Negative test: Active shader incorrectly designates c45 as WVP, and c45 contains
        valid perspective floats. Must be quarantined as BONE_PALETTE_BLOCK, never camera WVP."""
        proj = make_d3d_perspective_lh(fov_y_deg=60.0, aspect=1.0, zn=0.5, zf=200.0)
        c45_lines = "\n".join(f" c{45+i} {' '.join(str(proj[i*4 + j]) for j in range(4))}" for i in range(4))

        trace_text = f"""
0 op=1 a=0,0,0,0,0
 RT slot=0 size=3076x3140 format=113
1 op=20 a=45,165,0,0,100
{c45_lines}
2 op=19 a=1000,0,0,0,0
 VS BONE_TEST guessed=45,-1 named=-1,-1
3 op=14 a=4,0,0,100,0
"""
        with tempfile.NamedTemporaryFile("w", delete=False) as tf:
            tf.write(trace_text)
            temp_path = tf.name

        try:
            analyzer = TraceAnalyzer()
            analyzer.parse_file(temp_path)
            self.assertEqual(len(analyzer.classifications), 1)
            c = analyzer.classifications[0]
            self.assertEqual(c.evidence, EvidenceLevel.BONE_PALETTE_BLOCK)
            self.assertIn("Quarantined", c.notes[0])
            self.assertIsNone(c.fov_y_deg)
        finally:
            os.remove(temp_path)

    def test_stale_cross_pass_constants(self):
        """Ensures constants uploaded in earlier render pass are flagged STALE_CROSS_PASS,
        while legitimate intra-pass constants remain valid across draws."""
        proj = make_d3d_perspective_lh(fov_y_deg=60.0, aspect=1.0, zn=0.5, zf=200.0)
        c0_lines = "\n".join(f" c{i} {' '.join(str(proj[i*4 + j]) for j in range(4))}" for i in range(4))

        trace_text = f"""
0 op=1 a=0,100,0,0,0
 RT slot=0 size=4096x4096 format=23
1 op=20 a=0,4,0,0,10
{c0_lines}
2 op=19 a=1000,0,0,0,0
 VS SHADOW guessed=0,-1 named=-1,-1
3 op=14 a=4,0,0,10,0
4 op=1 a=0,200,0,0,0
 RT slot=0 size=3076x3140 format=113
5 op=19 a=2000,0,0,0,0
 VS SCENE_STALE guessed=0,-1 named=-1,-1
6 op=14 a=4,0,0,20,0
7 op=20 a=0,4,0,0,30
{c0_lines}
8 op=14 a=4,0,0,25,0
9 op=14 a=4,0,0,26,0
"""
        with tempfile.NamedTemporaryFile("w", delete=False) as tf:
            tf.write(trace_text)
            temp_path = tf.name

        try:
            analyzer = TraceAnalyzer()
            analyzer.parse_file(temp_path)
            # Draw 3: Shadow pass WVP
            # Draw 6: Stale cross-pass WVP (uploaded in shadow pass, read in scene pass)
            # Draw 8 & 9: Legitimate intra-pass WVP re-uploaded at cmd 7
            self.assertEqual(len(analyzer.classifications), 4)
            self.assertEqual(analyzer.classifications[0].evidence, EvidenceLevel.SHADOW_PASS_WVP)
            self.assertEqual(analyzer.classifications[1].evidence, EvidenceLevel.STALE_CROSS_PASS)
            self.assertEqual(analyzer.classifications[2].evidence, EvidenceLevel.PROJECTION_CONSISTENT_WVP)
            self.assertEqual(analyzer.classifications[3].evidence, EvidenceLevel.PROJECTION_CONSISTENT_WVP)
        finally:
            os.remove(temp_path)

    def test_rt_switching_and_mrt_isolation(self):
        """Validates that pass determination is governed strictly by RT0, not auxiliary MRTs,
        and unbinding RT0 resets pass to UNKNOWN_PASS."""
        trace_text = """
0 op=1 a=0,100,0,0,0
 RT slot=0 size=3076x3140 format=113
1 op=1 a=1,200,0,0,0
 RT slot=1 size=4096x4096 format=23
2 op=1 a=0,0,0,0,0
"""
        with tempfile.NamedTemporaryFile("w", delete=False) as tf:
            tf.write(trace_text)
            temp_path = tf.name

        try:
            analyzer = TraceAnalyzer()
            # Command 0 sets RT0
            analyzer.rt_slots[0] = (3076, 3140, 113)
            analyzer.update_pass_type()
            self.assertEqual(analyzer.current_pass, RenderPassType.MAIN_HDR_SCENE)

            # Command 1 sets MRT slot 1 to 4096x4096 - RT0 must NOT be overridden
            analyzer.rt_slots[1] = (4096, 4096, 23)
            analyzer.update_pass_type()
            self.assertEqual(analyzer.current_pass, RenderPassType.MAIN_HDR_SCENE)

            # Command 2 unbinds RT0
            analyzer.rt_slots.pop(0, None)
            analyzer.update_pass_type()
            self.assertEqual(analyzer.current_pass, RenderPassType.UNKNOWN_PASS)
        finally:
            os.remove(temp_path)

    def test_world_matrix_distinction(self):
        """Distinguishes VALIDATED_CAMERA_WVP (World=Identity verified),
        PROJECTION_CONSISTENT_WVP (unverified World), and CORRELATED_OBJECT_WVP (World != Identity)."""
        # Engine camera projection: Px = 1.768088, Py = 1.732051 (approx 60 deg)
        cam_proj = [
            1.768088, 0.0,      0.0,       0.0,
            0.0,      1.732051, 0.0,       0.0,
            0.0,      0.0,      1.0001,    1.0,
            0.0,      0.0,      -0.09501,  0.0,
        ]
        c0_cam = "\n".join(f" c{i} {' '.join(str(cam_proj[i*4 + j]) for j in range(4))}" for i in range(4))
        c4_ident = "\n".join(f" c{4+i} {' '.join(str(IDENTITY_4X4[i*4 + j]) for j in range(4))}" for i in range(4))
        c4_trans = "\n".join(f" c{4+i} {' '.join(str(TRANSLATION_4X4[i*4 + j]) for j in range(4))}" for i in range(4))

        trace_text = f"""
0 op=1 a=0,100,0,0,0
 RT slot=0 size=3076x3140 format=113
1 op=19 a=1000,0,0,0,0
 VS TEST guessed=0,-1 named=0,-1
2 op=20 a=0,8,0,0,10
{c0_cam}
{c4_ident}
3 op=14 a=4,0,0,10,0
4 op=20 a=0,8,0,0,20
{c0_cam}
{c4_trans}
5 op=14 a=4,0,0,20,0
6 op=20 a=0,4,0,0,30
{c0_cam}
7 op=14 a=4,0,0,30,0
"""
        with tempfile.NamedTemporaryFile("w", delete=False) as tf:
            tf.write(trace_text)
            temp_path = tf.name

        try:
            analyzer = TraceAnalyzer()
            analyzer.parse_file(temp_path)
            self.assertEqual(len(analyzer.classifications), 3)

            # Draw 3: W=Identity present -> VALIDATED_CAMERA_WVP
            self.assertEqual(analyzer.classifications[0].evidence, EvidenceLevel.VALIDATED_CAMERA_WVP)

            # Draw 5: W=Translation present -> CORRELATED_OBJECT_WVP
            self.assertEqual(analyzer.classifications[1].evidence, EvidenceLevel.CORRELATED_OBJECT_WVP)

            # Draw 7: Px/Py match camera, but no model matrix W -> PROJECTION_CONSISTENT_WVP
            self.assertEqual(analyzer.classifications[2].evidence, EvidenceLevel.PROJECTION_CONSISTENT_WVP)
        finally:
            os.remove(temp_path)

    def test_deterministic_synthetic_trace_fixture(self):
        """Runs a complete multi-pass synthetic trace fixture verifying all pipeline features."""
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
            # Pass 1 produces SHADOW_PASS_WVP
            # Pass 2 produces VALIDATED_CAMERA_WVP and SCREEN_TEX_PROJ
            evidences = [c.evidence for c in analyzer.classifications]
            self.assertIn(EvidenceLevel.SHADOW_PASS_WVP, evidences)
            self.assertIn(EvidenceLevel.VALIDATED_CAMERA_WVP, evidences)
            self.assertIn(EvidenceLevel.SCREEN_TEXTURE_PROJECTION, evidences)
        finally:
            os.remove(temp_path)

    def test_real_darkness2_trace_reconciliation(self):
        """Verifies regression against real Darkness II frame trace run_43052_frame_003_trace.txt.
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

if __name__ == "__main__":
    unittest.main()
