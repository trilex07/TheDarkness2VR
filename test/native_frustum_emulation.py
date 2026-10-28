"""Emulate only the two SHA-pinned frustum builders, never the game entrypoint.

Requires pefile, numpy and unicorn (tested with unicorn2.1.4). No dependencies
or proprietary binaries are vendored. EXE is read-only input; exported cases
contain only this repository's generated matrices/points. See Unicorn's API:
https://www.unicorn-engine.org/docs/tutorial.html
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--exe", required=True, type=Path)
parser.add_argument("--cases", required=True, type=Path)
parser.add_argument("--dependency-root", type=Path)
parser.add_argument("--report", type=Path)
args = parser.parse_args()
if args.dependency_root:
    sys.path.insert(0, str(args.dependency_root))
import numpy as np
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_MEM_WRITE
from unicorn.x86_const import (UC_X86_REG_ESP, UC_X86_REG_ECX, UC_X86_REG_EIP,
                              UC_X86_REG_FPCW, UC_X86_REG_MXCSR)


def require(value, message):
    if not value:
        raise RuntimeError(message)


sha = hashlib.sha256(args.exe.read_bytes()).hexdigest()
require(sha == "d46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1", "Wrong EXE SHA")
pe = pefile.PE(str(args.exe))
base = pe.OPTIONAL_HEADER.ImageBase
require(base == 0x400000 and pe.FILE_HEADER.Machine == 0x14c, "Wrong image layout")
uc = Uc(UC_ARCH_X86, UC_MODE_32)
uc.mem_map(base, (pe.OPTIONAL_HEADER.SizeOfImage + 4095) & ~4095)
uc.mem_write(base, pe.get_memory_mapped_image())
heap, stack, stop = 0x20000000, 0x30000000, 0x33000000
uc.mem_map(heap, 0x10000)
uc.mem_map(stack, 0x10000)
uc.mem_map(stop, 4096)


def write_u32(address, *values):
    uc.mem_write(address, struct.pack("<" + "I" * len(values), *values))


def read_u32(address, count):
    return struct.unpack("<" + "I" * count, uc.mem_read(address, 4 * count))


def floats(address, count):
    return np.array(struct.unpack("<" + "f" * count, uc.mem_read(address, 4 * count)))


def check_write(_uc, _access, address, size, _value, _data):
    # All original instructions run; no allocator/API stubs or skipped calls.
    # Only the owned frustum, preallocated vectors and private stack may change.
    ranges = ((heap, heap + 0x160), (heap + 0x2000, heap + 0x2060),
              (heap + 0x3000, heap + 0x3018), (stack, stack + 0x10000))
    require(any(lo <= address and address + size <= hi for lo, hi in ranges),
            f"Unexpected write outside frustum/vector/stack: {address:08x}")


uc.hook_add(UC_HOOK_MEM_WRITE, check_write)
write_u32(heap + 0x30, heap + 0x2000, heap + 0x2060, heap + 0x2060)
write_u32(heap + 0x150, heap + 0x3000, heap + 0x3018, heap + 0x3018)


def call(target, matrix):
    uc.mem_write(heap + 0x1000, matrix)
    sp = stack + 0xff00
    write_u32(sp, stop, heap + 0x1000)
    uc.reg_write(UC_X86_REG_ESP, sp)
    uc.reg_write(UC_X86_REG_ECX, heap)
    uc.reg_write(UC_X86_REG_FPCW, 0x37f)
    uc.reg_write(UC_X86_REG_MXCSR, 0x1f80)
    uc.emu_start(target, stop, count=100000)
    require(uc.reg_read(UC_X86_REG_EIP) == stop, "Instruction bound exceeded")
    require(uc.reg_read(UC_X86_REG_ESP) == sp + 8, "thiscall stack cleanup mismatch")
    require(uc.mem_read(heap + 0x1000, 64) == matrix, "Input matrix changed")


data = args.cases.read_bytes()
require(data[:8] in (b"D2FRC001", b"D2FRC002"), "Wrong generated-case format")
reference_eyes = data[:8] == b"D2FRC002"
count = struct.unpack_from("<I", data, 8)[0]
require(count == 240, "Incomplete generated cases")
offset, total = 12, 0
max_coeff = max_bounds = max_plane_outside = max_box_outside = 0.
reference_plane_max = np.full(6, -np.inf)
reference_box_max = -np.inf
reference_plane_cases = reference_box_cases = 0
clip_corners = np.array([[x, y, z, 1] for x in [-1, 1] for y in [-1, 1] for z in [0, 1]])
for case in range(count):
    if reference_eyes:
        view = np.frombuffer(data, dtype="<f4", count=16, offset=offset).reshape(4, 4).astype(float)
        eyes = np.frombuffer(data, dtype="<f4", count=32, offset=offset+64).reshape(2, 4, 4).astype(float)
        offset += 192
    vp, ivp = data[offset:offset+64], data[offset+64:offset+128]
    offset += 128
    n = struct.unpack_from("<I", data, offset)[0]
    offset += 4
    require(n == 1040, "Incomplete binocular samples")
    points = np.frombuffer(data, dtype="<f4", count=n*4, offset=offset).reshape(n, 4).astype(float)
    offset += n*16
    require(np.all(np.isfinite(points)) and np.all(points[:, 3] > 0), "Invalid generated point")
    points = points[:, :3] / points[:, 3:]
    call(0x607cd0, vp)
    planes = floats(heap + 0x2000, 24).reshape(6, 4)
    call(0x685050, ivp)
    bounds = floats(heap + 0x10, 8).reshape(2, 4)
    require(read_u32(heap + 0x3000, 6) == (1, 0, 0, 0, 0, 2), "Native plane types changed")
    require(read_u32(heap + 0x30, 3) == (heap+0x2000, heap+0x2060, heap+0x2060), "Plane ownership/capacity changed")
    require(read_u32(heap + 0x150, 3) == (heap+0x3000, heap+0x3018, heap+0x3018), "Type ownership/capacity changed")
    require(read_u32(heap, 2) == (1, 1), "Native bounds flags missing")
    require(np.all(np.isfinite(planes)) and np.all(np.isfinite(bounds)), "Nonfinite native output")
    # Independent D3D clip-plane oracle, evaluated in double precision.
    matrix = np.frombuffer(vp, dtype="<f4").reshape(4, 4).astype(float)
    raw = np.stack([matrix[:, 2], matrix[:, 3]+matrix[:, 0], matrix[:, 3]-matrix[:, 0],
                    matrix[:, 3]-matrix[:, 1], matrix[:, 3]+matrix[:, 1], matrix[:, 3]-matrix[:, 2]])
    expected = raw / np.linalg.norm(raw[:, :3], axis=1)[:, None]
    expected[:, 3] *= -1
    coeff_error = float(np.max(np.abs(planes-expected)/(1+np.abs(expected))))
    require(coeff_error <= 2e-6, f"Native plane convention mismatch case={case}")
    inverse = np.frombuffer(ivp, dtype="<f4").reshape(4, 4).astype(float)
    corners = np.array([[x, y, z, 1] for x in [-1, 1] for y in [-1, 1] for z in [0, 1]]) @ inverse
    require(np.all(corners[:, 3] > 0), "Unbounded inverse corners")
    xyz = corners[:, :3] / corners[:, 3:]
    expected_bounds = np.stack([np.min(xyz, axis=0), np.max(xyz, axis=0)])
    bounds_error = float(np.max(np.abs(bounds[:, :3]-expected_bounds)/(1+np.abs(expected_bounds))))
    require(bounds_error <= 1e-5, f"Native AABB convention mismatch case={case}")
    distances = points @ planes[:, :3].T - planes[:, 3]
    if reference_eyes:
        # Independent geometric reference: invert the ORIGINAL per-eye inputs
        # in double precision, not the rounded fitted world VP/inverse or the
        # float-generated test points. Report errors separately; legacy PASS
        # still means the explicitly loose geometry checks below, not strict
        # containment of these reference corners.
        inverse_view = np.linalg.inv(view)
        world = np.concatenate([clip_corners @ np.linalg.inv(eye) @ inverse_view for eye in eyes])
        require(np.all(np.isfinite(world)) and np.all(world[:, 3] > 0), "Invalid double reference")
        world = world[:, :3] / world[:, 3:]
        outside = -(world @ planes[:, :3].T - planes[:, 3])
        box_outside_reference = np.maximum(bounds[0, :3]-world, world-bounds[1, :3])
        reference_plane_max = np.maximum(reference_plane_max, np.max(outside, axis=0))
        reference_box_max = max(reference_box_max, float(np.max(box_outside_reference)))
        reference_plane_cases += int(np.any(outside > 1e-4))
        reference_box_cases += int(np.any(box_outside_reference > 1e-4))
    plane_outside = np.max(-distances, axis=1)
    box_outside = np.max(np.maximum(bounds[0, :3]-points, points-bounds[1, :3]), axis=1)
    # Same loose edge/depth tolerance as the existing geometry fixture. This
    # check is not exact containment; report the raw worst excursions too.
    tolerance = .003*(1+np.max(np.abs(points), axis=1))
    require(np.all(plane_outside <= tolerance) and np.all(box_outside <= tolerance),
            f"Native output exceeds geometry-fixture tolerance case={case}")
    max_coeff, max_bounds = max(max_coeff, coeff_error), max(max_bounds, bounds_error)
    max_plane_outside = max(max_plane_outside, float(np.max(plane_outside)))
    max_box_outside = max(max_box_outside, float(np.max(box_outside)))
    total += n
require(offset == len(data), "Trailing/truncated generated cases")
report = dict(status="PASS", executable_sha256=sha, cases=count, binocular_points=total,
              native_calls=2*count, plane_oracle_max_relative_error=max_coeff,
              bounds_oracle_max_relative_error=max_bounds,
              raw_max_plane_excursion_world_units=max_plane_outside,
              raw_max_aabb_excursion_world_units=max_box_outside,
              runtime_acceptance="NOT_RUN", game_launched=False,
              scope="Original x86 functions in emulator; preallocated vectors; not runtime ownership or HMD proof")
if reference_eyes:
    report["double_eye_reference"] = dict(
        plane_max_excursions_world_units=reference_plane_max.tolist(),
        aabb_max_excursion_world_units=reference_box_max,
        plane_cases_outside_1e_4=reference_plane_cases,
        aabb_cases_outside_1e_4=reference_box_cases,
        strict_corner_containment=bool(np.max(reference_plane_max) <= 0 and reference_box_max <= 0),
        interpretation="Diagnostic reference, not covered by legacy PASS; includes far/depth boundary errors",
        corners_per_pose=16)
if args.report:
    args.report.write_text(json.dumps(report, indent=2), encoding="utf-8")
print(json.dumps(report))
