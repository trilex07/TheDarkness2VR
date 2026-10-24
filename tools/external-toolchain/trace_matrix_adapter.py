#!/usr/bin/env python3
"""trace_matrix_adapter.py - Offline Matrix Classification & Camera Analysis Adapter

Bridges Darkness2VR record-stream traces (run_*_trace.txt) with the matrix
classification and camera-tracking methodology validated from apitrace-mcp.

Extracts candidate 4x4 shader constant blocks (vs_c registers), performs
frustum plane extraction (Gribb-Hartmann), classifies matrix kinds (viewproj,
rigid, ortho, projection), and recovers world-space camera eye and FOV.
"""

from __future__ import annotations

import argparse
import math
import os
import re
import sys
from collections import defaultdict
from dataclasses import dataclass, field
from pathlib import Path
from typing import Sequence

# --------------------------------------------------------------------------
# Standalone Matrix Classification Engine (Derived & Verified from apitrace-mcp)
# --------------------------------------------------------------------------

EPS_ZERO = 1e-4
EPS_ONE = 1e-3

Vec3 = tuple[float, float, float]
Vec4 = tuple[float, float, float, float]

def _dot3(a: Sequence[float], b: Sequence[float]) -> float:
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]

def _length3(v: Sequence[float]) -> float:
    return math.sqrt(max(_dot3(v, v), 0.0))

def _normalize3(v: Sequence[float]) -> Vec3:
    n = _length3(v)
    if n < 1e-20:
        return (0.0, 0.0, 0.0)
    return (v[0] / n, v[1] / n, v[2] / n)

def transpose16(m: Sequence[float]) -> list[float]:
    return [m[col * 4 + row] for row in range(4) for col in range(4)]

def _round(value: float, digits: int = 4) -> float:
    if not math.isfinite(value):
        return value
    return round(value, digits)

def _solve3(rows: Sequence[Sequence[float]], rhs: Sequence[float]) -> Vec3 | None:
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

@dataclass
class MatrixDecode:
    kind: str  # projection | rigid | viewproj | ortho | unknown
    confidence: float
    layout: str  # as_stored | transposed
    convention: str = "d3d"
    fov_y_deg: float | None = None
    fov_x_deg: float | None = None
    aspect: float | None = None
    eye: Vec3 | None = None
    axis_z: Vec3 | None = None
    notes: list[str] = field(default_factory=list)

def _decode_ortho(m: Sequence[float]) -> MatrixDecode | None:
    if max(abs(m[i]) for i in (1, 2, 3, 4, 6, 7, 8, 9, 11)) > EPS_ZERO:
        return None
    if abs(m[15] - 1.0) > EPS_ONE:
        return None
    sx, sy = m[0], m[5]
    if abs(sx) < 1e-6 or abs(sy) < 1e-6:
        return None
    dec = MatrixDecode(kind="ortho", confidence=1.0, layout="as_stored")
    dec.notes.append(f"w={_round(2.0/sx, 2)} h={_round(2.0/sy, 2)} (ortho/UI)")
    return dec

def _decode_rigid(m: Sequence[float]) -> MatrixDecode | None:
    if any(abs(m[i]) > EPS_ZERO for i in (3, 7, 11)):
        return None
    if abs(m[15] - 1.0) > EPS_ONE:
        return None
    ax = (m[0], m[1], m[2])
    ay = (m[4], m[5], m[6])
    az = (m[8], m[9], m[10])
    unit_err = abs(_length3(ax) - 1.0) + abs(_length3(ay) - 1.0) + abs(_length3(az) - 1.0)
    ortho_err = abs(_dot3(ax, ay)) + abs(_dot3(ay, az)) + abs(_dot3(az, ax))
    if unit_err > 0.08 or ortho_err > 0.08:
        return None
    t = (m[12], m[13], m[14])
    eye = (
        -(m[0] * t[0] + m[1] * t[1] + m[2] * t[2]),
        -(m[4] * t[0] + m[5] * t[1] + m[6] * t[2]),
        -(m[8] * t[0] + m[9] * t[1] + m[10] * t[2]),
    )
    dec = MatrixDecode(kind="rigid", confidence=1.0, layout="as_stored")
    dec.eye = tuple(_round(v) for v in eye)  # type: ignore[assignment]
    dec.axis_z = tuple(_round(v) for v in az)  # type: ignore[assignment]
    return dec

def _decode_viewproj(m: Sequence[float]) -> MatrixDecode | None:
    col = [tuple(m[row * 4 + c] for row in range(4)) for c in range(4)]
    c0, c1, c2, c3 = col
    if max(abs(v) for v in (m[3], m[7], m[11])) < EPS_ZERO:
        return None

    left = tuple(c3[i] + c0[i] for i in range(4))
    right = tuple(c3[i] - c0[i] for i in range(4))
    bottom = tuple(c3[i] + c1[i] for i in range(4))
    top = tuple(c3[i] - c1[i] for i in range(4))

    nl, nr = _normalize3(left), _normalize3(right)
    nb, nt = _normalize3(bottom), _normalize3(top)
    if min(_length3(left), _length3(right), _length3(top), _length3(bottom)) < 1e-12:
        return None

    def _angle(a: Vec3, b: Vec3) -> float | None:
        d = max(-1.0, min(1.0, _dot3(a, b)))
        ang = math.pi - math.acos(d)
        return math.degrees(ang) if 0.0 < ang < math.pi else None

    fov_y = _angle(nt, nb)
    fov_x = _angle(nl, nr)
    if fov_y is None or fov_x is None:
        return None
    if not (5.0 < fov_y < 175.0 and 5.0 < fov_x < 175.0):
        return None

    try:
        aspect = math.tan(math.radians(fov_x) / 2) / math.tan(math.radians(fov_y) / 2)
    except (ValueError, ZeroDivisionError):
        return None
    if not (0.1 < aspect < 10.0):
        return None

    eye = _solve3(
        [nl, nr, nt],
        [-left[3] / _length3(left), -right[3] / _length3(right), -top[3] / _length3(top)],
    )
    if eye is None:
        return None

    residual = abs(_dot3(nb, eye) + bottom[3] / _length3(bottom))
    if residual > 0.15:
        return None

    w_len = math.sqrt(c3[0] ** 2 + c3[1] ** 2 + c3[2] ** 2)
    az = _normalize3(c2[:3])

    dec = MatrixDecode(
        kind="viewproj",
        confidence=0.9 if abs(w_len - 1.0) < 0.1 else 0.75,
        layout="as_stored",
        fov_y_deg=_round(fov_y, 2),
        fov_x_deg=_round(fov_x, 2),
        aspect=_round(aspect, 4),
        eye=tuple(_round(v) for v in eye),  # type: ignore[assignment]
        axis_z=tuple(_round(v) for v in az),  # type: ignore[assignment]
    )
    if abs(w_len - 1.0) >= 0.1:
        dec.notes.append(f"w-scale={_round(w_len, 3)}")
    return dec

def classify_matrix(m: Sequence[float]) -> MatrixDecode | None:
    if len(m) != 16 or not all(math.isfinite(v) for v in m):
        return None

    for d in (_decode_ortho(m), _decode_rigid(m), _decode_viewproj(m)):
        if d:
            return d

    m_t = transpose16(m)
    for d in (_decode_ortho(m_t), _decode_rigid(m_t), _decode_viewproj(m_t)):
        if d:
            d.layout = "transposed"
            return d
    return None

# --------------------------------------------------------------------------
# Trace Stream Parser
# --------------------------------------------------------------------------

LINE_RE = re.compile(r"^\s*(\d+)\s+op=(\d+)\s+a=([0-9,\-]+)")
CONST_RE = re.compile(r"^\s*c(\d+)\s+([-\d.eE]+)\s+([-\d.eE]+)\s+([-\d.eE]+)\s+([-\d.eE]+)")

def analyze_trace_file(trace_path: str | Path, min_conf: float = 0.7):
    path = Path(trace_path)
    if not path.is_file():
        print(f"Error: file not found: {path}", file=sys.stderr)
        return False

    constants: dict[int, list[float]] = {}
    upload_aligned_bases: set[int] = set()
    draw_calls = 0
    matrices_by_slot: dict[int, list[MatrixDecode]] = defaultdict(list)
    slot_draw_count: dict[int, int] = defaultdict(int)

    with path.open("r", encoding="utf-8", errors="replace") as f:
        for line in f:
            m = LINE_RE.match(line)
            if m:
                call_idx = int(m.group(1))
                op = int(m.group(2))
                args = [int(x) for x in m.group(3).split(",")]

                if op == 20:  # SetVertexShaderConstantF
                    start, count = args[0], args[1]
                    for k in range(0, count, 4):
                        if k + 4 <= count:
                            upload_aligned_bases.add(start + k)

                elif op in (13, 14, 15, 16):  # DrawPrimitive / DrawIndexedPrimitive
                    draw_calls += 1
                    for base in sorted(upload_aligned_bases):
                        if all((base + i) in constants for i in range(4)):
                            mat = []
                            for i in range(4):
                                mat.extend(constants[base + i])
                            dec = classify_matrix(mat)
                            if dec and dec.confidence >= min_conf:
                                matrices_by_slot[base].append(dec)
                                slot_draw_count[base] += 1
                continue

            c = CONST_RE.match(line)
            if c:
                reg = int(c.group(1))
                vals = [float(c.group(i)) for i in range(2, 6)]
                constants[reg] = vals

    print("=" * 80)
    print(f" Darkness2VR Offline Matrix Analysis: {path.name}")
    print(f" Processed {draw_calls} draw calls; {len(upload_aligned_bases)} aligned matrix upload slots")
    print("=" * 80)

    if not matrices_by_slot:
        print("No classified 4x4 matrices found with confidence >= ", min_conf)
        return True

    print(f"{'Slot':<12} {'Kind':<10} {'Layout':<11} {'Conf':<6} {'Draws':<6} {'FOV(H x V)':<16} {'Camera Eye (X, Y, Z)':<25}")
    print("-" * 80)

    for slot in sorted(matrices_by_slot.keys()):
        decs = matrices_by_slot[slot]
        count = slot_draw_count[slot]
        primary = decs[-1]

        fov_str = f"{primary.fov_x_deg or '-'} x {primary.fov_y_deg or '-'}"
        eye_str = f"{primary.eye}" if primary.eye else "-"
        print(f"c{slot}..c{slot+3:<6} {primary.kind:<10} {primary.layout:<11} {primary.confidence:<6.2f} {count:<6} {fov_str:<16} {eye_str:<25}")

    print("=" * 80)
    return True

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Analyze matrices in Darkness2VR record traces")
    parser.add_argument("trace", help="Path to run_*_trace.txt")
    parser.add_argument("--min-conf", type=float, default=0.7, help="Minimum confidence threshold (0.0 to 1.0)")
    args = parser.parse_args()

    analyze_trace_file(args.trace, args.min_conf)
