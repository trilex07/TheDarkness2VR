#!/usr/bin/env python3
"""test_trace_matrix_adapter.py - Deterministic Tests for Matrix Trace Adapter

Tests cover:
  1. Positive validation of standard D3D9 perspective matrices and camera frustum solving.
  2. Camera apex (eye) recovery from known View-Projection matrices.
  3. Negative test: Skeletal bone palette uploads (e.g. 165 constants) are excluded
     and never misidentified as gameplay cameras.
  4. Negative test: Stale constants left over from an earlier pass are not reported
     at subsequent draws using shaders that do not reference them.
  5. Pass discrimination: Shadow atlas passes (4096x4096 R32F) are distinguished
     from main HDR scene passes (3076x3140).
  6. Regression against real Darkness2VR frame trace (run_43052_frame_003_trace.txt)
     reconciling camera coefficients (Px=1.768, Py=1.732 -> 60 deg vertical FOV).
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

class TestTraceMatrixAdapter(unittest.TestCase):

    def test_synthetic_camera_projection(self):
        """Validates frustum plane extraction on pure 60-degree D3D9 perspective matrix."""
        proj = make_d3d_perspective_lh(fov_y_deg=60.0, aspect=1.5, zn=0.1, zf=100.0)
        res = solve_camera_frustum(proj)
        self.assertIsNotNone(res)
        fov_x, fov_y, aspect, eye, vz = res
        self.assertAlmostEqual(fov_y, 60.0, places=2)
        self.assertAlmostEqual(aspect, 1.5, places=2)
        # For pure projection at origin, eye is at (0, 0, 0)
        for val in eye:
            self.assertAlmostEqual(val, 0.0, places=2)

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
        """Ensures that a 165-register bone palette upload is never reported as camera WVP."""
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
            # Bone palette registers must be tracked and no camera WVP reported
            self.assertEqual(len(analyzer.bone_palette_registers), 165)
            self.assertEqual(len(analyzer.classifications), 0)
        finally:
            os.remove(temp_path)

    def test_negative_stale_constant_isolation(self):
        """Ensures stale constants from pass A are not re-reported at pass B draws without reference."""
        proj = make_d3d_perspective_lh(fov_y_deg=60.0, aspect=1.0, zn=0.5, zf=200.0)
        c0_lines = "\n".join(f" c{i} {' '.join(str(proj[i*4 + j]) for j in range(4))}" for i in range(4))

        trace_text = f"""
0 op=1 a=0,0,0,0,0
 RT slot=0 size=3076x3140 format=113
1 op=19 a=1000,0,0,0,0
 VS PASS_A guessed=0,-1 named=-1,-1
2 op=20 a=0,4,0,0,100
{c0_lines}
3 op=14 a=4,0,0,100,0
4 op=19 a=2000,0,0,0,0
 VS PASS_B guessed=-1,-1 named=-1,-1
5 op=14 a=4,0,0,50,0
"""
        with tempfile.NamedTemporaryFile("w", delete=False) as tf:
            tf.write(trace_text)
            temp_path = tf.name

        try:
            analyzer = TraceAnalyzer()
            analyzer.parse_file(temp_path)
            # Only 1 draw call should have reported a WVP (draw 3 in PASS_A), not draw 5 in PASS_B
            self.assertEqual(len(analyzer.classifications), 1)
            self.assertEqual(analyzer.classifications[0].draw_command, 3)
            self.assertEqual(analyzer.classifications[0].vs_hash, "PASS_A")
        finally:
            os.remove(temp_path)

    def test_shadow_atlas_pass_discrimination(self):
        """Ensures matrices uploaded during 4096x4096 R32F passes are tagged SHADOW_PASS_WVP."""
        proj = make_d3d_perspective_lh(fov_y_deg=90.0, aspect=1.0, zn=0.1, zf=50.0)
        c0_lines = "\n".join(f" c{i} {' '.join(str(proj[i*4 + j]) for j in range(4))}" for i in range(4))

        trace_text = f"""
0 op=1 a=0,0,0,0,0
 RT slot=0 size=4096x4096 format=23
1 op=19 a=1000,0,0,0,0
 VS SHADOW guessed=0,-1 named=-1,-1
2 op=20 a=0,4,0,0,100
{c0_lines}
3 op=14 a=4,0,0,100,0
"""
        with tempfile.NamedTemporaryFile("w", delete=False) as tf:
            tf.write(trace_text)
            temp_path = tf.name

        try:
            analyzer = TraceAnalyzer()
            analyzer.parse_file(temp_path)
            self.assertEqual(len(analyzer.classifications), 1)
            self.assertEqual(analyzer.classifications[0].evidence, EvidenceLevel.SHADOW_PASS_WVP)
            self.assertEqual(analyzer.classifications[0].pass_type, RenderPassType.SHADOW_ATLAS)
        finally:
            os.remove(temp_path)

    def test_real_darkness2_trace_reconciliation(self):
        """Verifies regression against real Darkness II frame trace run_43052_frame_003_trace.txt."""
        real_trace = Path("C:/Users/Trilex/Darkness2VR/test-output/run_43052_frame_003_trace.txt")
        if not real_trace.is_file():
            self.skipTest("Real trace file not found at " + str(real_trace))

        analyzer = TraceAnalyzer()
        analyzer.parse_file(real_trace)

        # 1. Bone palette registers must be detected
        self.assertIn(45, analyzer.bone_palette_registers)
        self.assertIn(209, analyzer.bone_palette_registers)

        # 2. Confirmed camera WVP must be detected with 60 deg vertical FOV
        confirmed_cameras = [c for c in analyzer.classifications if c.evidence == EvidenceLevel.PROVENANCE_CONFIRMED_CAMERA_WVP]
        self.assertGreater(len(confirmed_cameras), 0)
        for cam in confirmed_cameras:
            self.assertAlmostEqual(cam.fov_y_deg, 60.0, delta=1.5)
            self.assertAlmostEqual(cam.fov_x_deg, 58.98, delta=1.5)

        # 3. Shadow passes must be distinguished
        shadow_wvps = [c for c in analyzer.classifications if c.evidence == EvidenceLevel.SHADOW_PASS_WVP]
        self.assertGreater(len(shadow_wvps), 0)
        self.assertEqual(shadow_wvps[0].pass_type, RenderPassType.SHADOW_ATLAS)

if __name__ == "__main__":
    unittest.main()
