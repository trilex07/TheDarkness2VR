#!/usr/bin/env python3
"""trace_matrix_adapter.py - Accurate D3D9 Trace Matrix & Camera Provenance Analyzer

Analyzes Darkness2VR record-stream traces (run_*_trace.txt) by correlating
shader constant uploads with active vertex shader bytecode reflection, CTAB
constant table semantics, render target dimensions/formats, and draw call context.

Mathematical Foundations (Clean-room implementations of published literature):
  - Frustum plane extraction: Gribb, G. & Hartmann, K. (2001),
    'Fast Extraction of Viewing Frustum Planes from the World-View-Projection Matrix'.
  - Direct3D 9 row-vector transformations and projection scaling:
    Microsoft Direct3D 9 Graphics Pipeline Documentation.
"""

from __future__ import annotations

import argparse
import math
import os
import re
import sys
from collections import defaultdict
from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path
from typing import Sequence

# --------------------------------------------------------------------------
# Evidence & Provenance Levels
# --------------------------------------------------------------------------

class EvidenceLevel(str, Enum):
    PROVENANCE_CONFIRMED_CAMERA_WVP = "CONFIRMED_CAMERA_WVP"
    PROVENANCE_CORRELATED_WVP = "CORRELATED_OBJECT_WVP"
    SHADOW_PASS_WVP = "SHADOW_PASS_WVP"
    SCREEN_TEXTURE_PROJECTION = "SCREEN_TEX_PROJ"
    MODEL_TRANSFORM_AFFINE = "MODEL_RIGID_AFFINE"
    ORTHO_UI = "ORTHO_UI"
    BONE_PALETTE_BLOCK = "BONE_PALETTE"
    UNVERIFIED_CANDIDATE = "UNVERIFIED_CANDIDATE"
    UNKNOWN = "UNKNOWN"

class RenderPassType(str, Enum):
    SHADOW_ATLAS = "SHADOW_ATLAS (4096x4096 R32F)"
    MAIN_HDR_SCENE = "MAIN_HDR_SCENE (3076x3140 FP16)"
    MAIN_FORWARD = "MAIN_FORWARD (3076x3140 RGB)"
    POSTFX_DOWNSAMPLE = "POSTFX_DOWNSAMPLE"
    UNKNOWN_PASS = "UNKNOWN_PASS"

# --------------------------------------------------------------------------
# Clean-Room Linear Algebra & Frustum Mathematics
# --------------------------------------------------------------------------

Vec3 = tuple[float, float, float]
Vec4 = tuple[float, float, float, float]

def dot3(a: Sequence[float], b: Sequence[float]) -> float:
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]

def length3(v: Sequence[float]) -> float:
    return math.sqrt(max(dot3(v, v), 0.0))

def normalize3(v: Sequence[float]) -> Vec3:
    n = length3(v)
    if n < 1e-12:
        return (0.0, 0.0, 0.0)
    return (v[0] / n, v[1] / n, v[2] / n)

def transpose16(m: Sequence[float]) -> list[float]:
    return [m[col * 4 + row] for row in range(4) for col in range(4)]

def solve_3x3(rows: Sequence[Sequence[float]], rhs: Sequence[float]) -> Vec3 | None:
    """Solves a 3x3 linear system A * x = rhs via Cramer's rule."""
    def det3(a: Sequence[Sequence[float]]) -> float:
        return (
            a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1])
            - a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0])
            + a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0])
        )
    base = det3(rows)
    if abs(base) < 1e-12:
        return None
    out = []
    for col in range(3):
        swapped = [list(r) for r in rows]
        for r in range(3):
            swapped[r][col] = rhs[r]
        out.append(det3(swapped) / base)
    return (out[0], out[1], out[2])

# --------------------------------------------------------------------------
# Matrix Classification & Decomposition
# --------------------------------------------------------------------------

@dataclass
class MatrixClassification:
    evidence: EvidenceLevel
    layout: str  # "row_major" or "transposed"
    pass_type: RenderPassType
    slot: str  # e.g. "c0..c3"
    draw_command: int
    vs_hash: str
    fov_x_deg: float | None = None
    fov_y_deg: float | None = None
    proj_px: float | None = None
    proj_py: float | None = None
    aspect: float | None = None
    eye_pos: Vec3 | None = None
    view_z_dir: Vec3 | None = None
    notes: list[str] = field(default_factory=list)

def extract_gribb_hartmann_planes_d3d(m: Sequence[float]) -> tuple[Vec4, Vec4, Vec4, Vec4] | None:
    """Extract Left, Right, Bottom, Top frustum planes for D3D row-major matrix:
    clip = [x, y, z, 1] * M.
    Left:   col3 + col0
    Right:  col3 - col0
    Bottom: col3 + col1
    Top:    col3 - col1
    """
    col0 = (m[0], m[4], m[8], m[12])
    col1 = (m[1], m[5], m[9], m[13])
    col3 = (m[3], m[7], m[11], m[15])

    left = (col3[0] + col0[0], col3[1] + col0[1], col3[2] + col0[2], col3[3] + col0[3])
    right = (col3[0] - col0[0], col3[1] - col0[1], col3[2] - col0[2], col3[3] - col0[3])
    bottom = (col3[0] + col1[0], col3[1] + col1[1], col3[2] + col1[2], col3[3] + col1[3])
    top = (col3[0] - col1[0], col3[1] - col1[1], col3[2] - col1[2], col3[3] - col1[3])

    if any(length3(p[:3]) < 1e-6 for p in (left, right, bottom, top)):
        return None
    return left, right, bottom, top

def solve_camera_frustum(m: Sequence[float]) -> tuple[float, float, float, Vec3, Vec3] | None:
    """Solves for FOV_x, FOV_y, aspect, camera eye, and view Z direction.
    Returns (fov_x, fov_y, aspect, eye, view_z) or None.
    """
    planes = extract_gribb_hartmann_planes_d3d(m)
    if not planes:
        return None
    left, right, bottom, top = planes

    nl = normalize3(left[:3])
    nr = normalize3(right[:3])
    nb = normalize3(bottom[:3])
    nt = normalize3(top[:3])

    cos_h = max(-1.0, min(1.0, dot3(nl, nr)))
    cos_v = max(-1.0, min(1.0, dot3(nb, nt)))
    ang_h = math.pi - math.acos(cos_h)
    ang_v = math.pi - math.acos(cos_v)

    if not (math.radians(5.0) < ang_h < math.radians(175.0)) or not (math.radians(5.0) < ang_v < math.radians(175.0)):
        return None

    fov_x = math.degrees(ang_h)
    fov_y = math.degrees(ang_v)
    try:
        aspect = math.tan(ang_h / 2) / math.tan(ang_v / 2)
    except ZeroDivisionError:
        return None

    # Solve apex intersection of left, right, top planes
    eye = solve_3x3(
        [nl, nr, nt],
        [-left[3] / length3(left[:3]), -right[3] / length3(right[:3]), -top[3] / length3(top[:3])]
    )
    if eye is None:
        return None

    # Verify apex on bottom plane to eliminate degenerate false positives
    bottom_dist = abs(dot3(nb, eye) + bottom[3] / length3(bottom[:3]))
    if bottom_dist > 0.2:
        return None

    col2 = (m[2], m[6], m[10])
    view_z = normalize3(col2)
    return fov_x, fov_y, aspect, eye, view_z

def is_rigid_affine(m: Sequence[float]) -> bool:
    """Checks if a 4x4 matrix represents a rigid/affine 3D transform (rotation + translation)."""
    if abs(m[3]) > 1e-4 or abs(m[7]) > 1e-4 or abs(m[11]) > 1e-4 or abs(m[15] - 1.0) > 1e-3:
        return False
    r0, r1, r2 = m[0:3], m[4:7], m[8:11]
    l0, l1, l2 = length3(r0), length3(r1), length3(r2)
    if abs(l0 - 1.0) > 0.05 or abs(l1 - 1.0) > 0.05 or abs(l2 - 1.0) > 0.05:
        return False
    if abs(dot3(r0, r1)) > 0.05 or abs(dot3(r1, r2)) > 0.05 or abs(dot3(r2, r0)) > 0.05:
        return False
    return True

def is_ortho(m: Sequence[float]) -> bool:
    """Checks if a 4x4 matrix is a 2D orthographic projection."""
    off_diag = (1, 2, 3, 4, 6, 7, 8, 9, 11)
    if any(abs(m[i]) > 1e-4 for i in off_diag):
        return False
    if abs(m[15] - 1.0) > 1e-3:
        return False
    if abs(m[0]) < 1e-6 or abs(m[5]) < 1e-6:
        return False
    return True

# --------------------------------------------------------------------------
# Trace Stream Processor with State & Pass Tracking
# --------------------------------------------------------------------------

LINE_RE = re.compile(r"^\s*(\d+)\s+op=(\d+)\s+a=([0-9,\-]+)")
CONST_RE = re.compile(r"^\s*c(\d+)\s+([-\d.eE]+)\s+([-\d.eE]+)\s+([-\d.eE]+)\s+([-\d.eE]+)")
RT_RE = re.compile(r"^\s*RT\s+slot=(\d+)\s+size=(\d+)x(\d+)\s+format=(\d+)")
VS_RE = re.compile(r"^\s*VS\s+(\w+)\s+guessed=([-\d]+),([-\d]+)\s+named=([-\d]+),([-\d]+)")
PS_RE = re.compile(r"^\s*PS\s+inverse=([-\d]+)\s+hash=(\w+)")

class TraceAnalyzer:
    def __init__(self):
        self.constants: dict[int, list[float]] = {}
        self.bone_palette_registers: set[int] = set()
        self.recent_uploads: dict[int, int] = {}  # reg_base -> cmd_idx

        self.current_rt_size: tuple[int, int] = (0, 0)
        self.current_rt_format: int = 0
        self.current_pass: RenderPassType = RenderPassType.UNKNOWN_PASS

        self.active_vs_hash: str = "NONE"
        self.active_vs_guessed_wvp: int = -1
        self.active_vs_guessed_stp: int = -1
        self.active_vs_named_wvp: int = -1
        self.active_vs_named_stp: int = -1

        self.active_ps_hash: str = "NONE"
        self.active_ps_inverse: int = -1

        self.classifications: list[MatrixClassification] = []

    def update_pass_type(self):
        w, h = self.current_rt_size
        fmt = self.current_rt_format
        if w == 4096 and h == 4096 and fmt == 23:
            self.current_pass = RenderPassType.SHADOW_ATLAS
        elif w == 3076 and h == 3140 and fmt == 113:
            self.current_pass = RenderPassType.MAIN_HDR_SCENE
        elif w == 3076 and h == 3140 and fmt in (21, 22):
            self.current_pass = RenderPassType.MAIN_FORWARD
        elif w > 0 and (w <= 1538 or h <= 1570):
            self.current_pass = RenderPassType.POSTFX_DOWNSAMPLE
        else:
            self.current_pass = RenderPassType.UNKNOWN_PASS

    def process_draw_call(self, cmd_idx: int):
        # 1. Identify primary candidate WVP slot from active VS metadata
        wvp_base = self.active_vs_named_wvp if self.active_vs_named_wvp >= 0 else self.active_vs_guessed_wvp

        if wvp_base >= 0:
            if all((wvp_base + i) in self.constants for i in range(4)):
                mat = []
                for i in range(4):
                    mat.extend(self.constants[wvp_base + i])

                res = solve_camera_frustum(mat)
                layout = "row_major"
                if not res:
                    mat_t = transpose16(mat)
                    res = solve_camera_frustum(mat_t)
                    if res:
                        mat = mat_t
                        layout = "transposed"

                if res:
                    fov_x, fov_y, aspect, eye, vz = res
                    col0 = mat[0:3] if layout == "transposed" else (mat[0], mat[4], mat[8])
                    col1 = mat[4:7] if layout == "transposed" else (mat[1], mat[5], mat[9])
                    col3 = mat[12:15] if layout == "transposed" else (mat[3], mat[7], mat[11])
                    len3 = length3(col3)
                    px = length3(col0) / len3 if len3 > 1e-6 else None
                    py = length3(col1) / len3 if len3 > 1e-6 else None

                    # Check for Model matrix in adjacent registers (LitInstance struct)
                    model_rigid = False
                    if all((wvp_base + 4 + i) in self.constants for i in range(4)):
                        m_cand = []
                        for i in range(4):
                            m_cand.extend(self.constants[wvp_base + 4 + i])
                        if is_rigid_affine(m_cand):
                            model_rigid = True

                    # Classify evidence level based on pass and provenance
                    if self.current_pass == RenderPassType.SHADOW_ATLAS:
                        ev = EvidenceLevel.SHADOW_PASS_WVP
                        note = "Shadow depth pass (4096x4096 R32F atlas)"
                    elif self.current_pass in (RenderPassType.MAIN_HDR_SCENE, RenderPassType.MAIN_FORWARD):
                        # Pure camera WVP has W = Identity, where px ~= 1.768 and py ~= 1.732 (60 deg FOV)
                        if px and py and abs(px - 1.768) < 0.05 and abs(py - 1.732) < 0.05:
                            ev = EvidenceLevel.PROVENANCE_CONFIRMED_CAMERA_WVP
                            note = "Matches documented engine camera projection diagonal (Px=1.768, Py=1.732, V-FOV=60.0 deg)"
                        elif model_rigid:
                            ev = EvidenceLevel.PROVENANCE_CORRELATED_WVP
                            note = "Combined WVP carrying non-identity model transform W (LitInstance c4..c7); plane angles are object-space"
                        else:
                            ev = EvidenceLevel.PROVENANCE_CORRELATED_WVP
                            note = f"Active VS {self.active_vs_hash} WVP; plane angles may carry local object scaling"
                    else:
                        ev = EvidenceLevel.UNVERIFIED_CANDIDATE
                        note = f"Uploaded in pass {self.current_pass.value}"

                    self.classifications.append(MatrixClassification(
                        evidence=ev,
                        layout=layout,
                        pass_type=self.current_pass,
                        slot=f"c{wvp_base}..c{wvp_base+3}",
                        draw_command=cmd_idx,
                        vs_hash=self.active_vs_hash,
                        fov_x_deg=round(fov_x, 2),
                        fov_y_deg=round(fov_y, 2),
                        proj_px=round(px, 4) if px else None,
                        proj_py=round(py, 4) if py else None,
                        aspect=round(aspect, 4),
                        eye_pos=tuple(round(v, 2) for v in eye),  # type: ignore[assignment]
                        view_z_dir=tuple(round(v, 4) for v in vz),  # type: ignore[assignment]
                        notes=[note]
                    ))

        # 2. Check for Screen Texture Projection if designated by VS
        stp_base = self.active_vs_named_stp if self.active_vs_named_stp >= 0 else self.active_vs_guessed_stp
        if stp_base >= 0 and all((stp_base + i) in self.constants for i in range(4)):
            self.classifications.append(MatrixClassification(
                evidence=EvidenceLevel.SCREEN_TEXTURE_PROJECTION,
                layout="row_major",
                pass_type=self.current_pass,
                slot=f"c{stp_base}..c{stp_base+3}",
                draw_command=cmd_idx,
                vs_hash=self.active_vs_hash,
                notes=[f"VS designated ScreenTextureProjection slot (guessed/named {stp_base})"]
            ))

    def parse_file(self, trace_path: str | Path):
        path = Path(trace_path)
        with path.open("r", encoding="utf-8", errors="replace") as f:
            for line in f:
                # 1. Parse Comments with Render Target, VS, or PS metadata
                rt_m = RT_RE.match(line)
                if rt_m:
                    self.current_rt_size = (int(rt_m.group(2)), int(rt_m.group(3)))
                    self.current_rt_format = int(rt_m.group(4))
                    self.update_pass_type()
                    continue

                vs_m = VS_RE.match(line)
                if vs_m:
                    self.active_vs_hash = vs_m.group(1)
                    self.active_vs_guessed_wvp = int(vs_m.group(2))
                    self.active_vs_guessed_stp = int(vs_m.group(3))
                    self.active_vs_named_wvp = int(vs_m.group(4))
                    self.active_vs_named_stp = int(vs_m.group(5))
                    continue

                ps_m = PS_RE.match(line)
                if ps_m:
                    self.active_ps_inverse = int(ps_m.group(1))
                    self.active_ps_hash = ps_m.group(2)
                    continue

                # 2. Parse Commands
                cmd_m = LINE_RE.match(line)
                if cmd_m:
                    cmd_idx = int(cmd_m.group(1))
                    op = int(cmd_m.group(2))
                    args = [int(x) for x in cmd_m.group(3).split(",")]

                    if op == 20:  # SetVertexShaderConstantF
                        start, count = args[0], args[1]
                        if count > 16:
                            # Skeletal bone palette upload - mark registers to prevent false sliding windows
                            for r in range(start, start + count):
                                self.bone_palette_registers.add(r)
                        else:
                            # Standard uniform upload
                            self.recent_uploads[start] = cmd_idx

                    elif op in (13, 14, 15, 16):  # Draw operations
                        self.process_draw_call(cmd_idx)
                    continue

                # 3. Parse Float Constants
                c_m = CONST_RE.match(line)
                if c_m:
                    reg = int(c_m.group(1))
                    vals = [float(c_m.group(i)) for i in range(2, 6)]
                    self.constants[reg] = vals

    def print_summary(self):
        print("=" * 100)
        print(" DARKNESS2VR PROVENANCE-AWARE MATRIX ANALYSIS")
        print("=" * 100)
        if not self.classifications:
            print("No correlated 3D draw matrices identified.")
            return

        # Group by (slot, evidence, pass_type)
        grouped = defaultdict(list)
        for c in self.classifications:
            grouped[(c.slot, c.evidence, c.pass_type)].append(c)

        print(f"{'Slot':<10} {'Evidence Level':<26} {'Pass Context':<25} {'FOV(H x V)':<16} {'Camera Eye (X, Y, Z)':<25}")
        print("-" * 105)

        for (slot, ev, p_type), items in sorted(grouped.items(), key=lambda x: str(x[0])):
            rep = items[-1]
            fov_str = f"{rep.fov_x_deg or '-'} x {rep.fov_y_deg or '-'}"
            eye_str = f"{rep.eye_pos}" if rep.eye_pos else "-"
            print(f"{slot:<10} {ev.value:<26} {p_type.name:<25} {fov_str:<16} {eye_str:<25}")
            if rep.notes:
                print(f"  --> Note: {rep.notes[0]} (Observed in {len(items)} draws)")

        print("=" * 100)

def main():
    parser = argparse.ArgumentParser(description="Analyze matrices in Darkness2VR traces with full provenance")
    parser.add_argument("trace", help="Path to run_*_trace.txt")
    args = parser.parse_args()

    analyzer = TraceAnalyzer()
    analyzer.parse_file(args.trace)
    analyzer.print_summary()

if __name__ == "__main__":
    main()
