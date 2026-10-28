"""Reproduce the exact pinned native projection-upload block in an emulator.

Reads an owned EXE as data; never starts/attaches to the game. No native bytes
are exported. Requires pefile and Unicorn; --deps can name an existing install.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import sys

parser = argparse.ArgumentParser()
parser.add_argument('--exe', type=Path, required=True)
parser.add_argument('--log', type=Path, required=True)
parser.add_argument('--deps', type=Path)
parser.add_argument('--report', type=Path)
args = parser.parse_args()
if args.deps:
    sys.path.insert(0, str(args.deps))
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_MEM_WRITE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_MXCSR

exe_sha = hashlib.sha256(args.exe.read_bytes()).hexdigest()
assert exe_sha == 'd46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1'
pe = pefile.PE(str(args.exe))
base = pe.OPTIONAL_HEADER.ImageBase
assert base == 0x400000 and pe.FILE_HEADER.Machine == 0x14c
assert hashlib.sha256(pe.get_data(0xad4021-base, 0x4b)).hexdigest() == \
    'a0cae25a9ffff9e07deeba8621ccfa228fd5de6a2229a0e2b0d4d71b8bc91b15'
assert struct.unpack('<f', pe.get_data(0xf185c8-base, 4))[0] == struct.unpack('<f', struct.pack('<f', .95))[0]

def inverse(matrix):
    a = [[float(matrix[r*4+c]) for c in range(4)] + [float(r == c) for c in range(4)] for r in range(4)]
    for c in range(4):
        pivot = max(range(c, 4), key=lambda r: abs(a[r][c]))
        a[c], a[pivot] = a[pivot], a[c]
        divisor = a[c][c]
        assert abs(divisor) > 1e-10
        a[c] = [v/divisor for v in a[c]]
        for r in range(4):
            if r != c:
                factor = a[r][c]
                a[r] = [x-factor*y for x, y in zip(a[r], a[c])]
    return [v for row in a for v in row[4:]]

uc = Uc(UC_ARCH_X86, UC_MODE_32)
uc.mem_map(base, (pe.OPTIONAL_HEADER.SizeOfImage+4095) & ~4095)
uc.mem_write(base, pe.get_memory_mapped_image())
stack = 0x30000000
uc.mem_map(stack, 0x10000)
sp = stack+0x8000
def writes(_uc, _access, address, size, _value, _data):
    assert stack <= address and address+size <= stack+0x10000, 'write outside emulated private stack'
uc.hook_add(UC_HOOK_MEM_WRITE, writes)

pattern = re.compile(r'phase=upload .*?native=\[([^]]+)\] upload=\[([^]]+)\]')
cases = 0
max_error = 0.
for match in pattern.finditer(args.log.read_text(encoding='utf-8-sig')):
    native, observed = ([float(x) for x in text.replace(';', ' ').split()] for text in match.groups())
    assert len(native) == len(observed) == 16
    # Reconstruct the forward camera matrix from the full nine-digit log.
    # This is replay of captured values, not a new simultaneous GPU capture.
    projection = struct.pack('<16f', *inverse(native))
    uc.mem_write(sp+0x450, projection)
    uc.reg_write(UC_X86_REG_ESP, sp)
    uc.reg_write(UC_X86_REG_MXCSR, 0x1f80)
    # Actual MOVSS/MULSS(.95)/store + BBE8F0 inversion, stopping at its caller.
    uc.emu_start(0xad403e, 0xad406c, count=10000)
    assert uc.reg_read(UC_X86_REG_EIP) == 0xad406c and uc.reg_read(UC_X86_REG_ESP) == sp
    result = struct.unpack('<16f', uc.mem_read(sp+0x5d0, 64))
    for actual, expected in zip(result, observed):
        error = abs(actual-expected)/(1+abs(expected))
        max_error = max(max_error, error)
        assert error <= 2e-4, (cases, actual, expected)
    copied = struct.unpack('<16f', uc.mem_read(sp+0x450, 64))
    original = struct.unpack('<16f', projection)
    assert all(copied[i] == original[i] for i in range(16) if i != 14)
    cases += 1
assert cases > 0, 'No original rejected upload pairs found in supplied log'
report = {'status': 'PASS_NATIVE_UPLOAD_REPRODUCTION_NOT_HMD', 'exe_sha256': exe_sha,
          'log_sha256': hashlib.sha256(args.log.read_bytes()).hexdigest(), 'captured_pairs': cases,
          'max_relative_component_error': max_error, 'native_block': 'AD403E..AD406C + BBE8F0',
          'writes': 'emulated private stack only', 'limitations': 'Logged matrix rounding; no live game/HMD/FPS proof'}
if args.report:
    args.report.write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
print(json.dumps(report))
