"""Verify the complete native clone plan against the user's stock EXE; no writes."""
from pathlib import Path
import hashlib
import re
import struct
import capstone
import pefile

exe = Path(r'C:\Program Files (x86)\Steam\steamapps\common\Darkness II\DarknessII.exe')
assert hashlib.sha256(exe.read_bytes()).hexdigest() == 'd46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1'
pe = pefile.PE(str(exe))
read = lambda va, n: pe.get_data(va - 0x400000, n)
plan = (Path(__file__).resolve().parent.parent / 'src/camera_source_clone_plan.h').read_text()
blocks = [(int(a,16),int(n,16),sha) for a,n,sha in re.findall(r'\{reinterpret_cast<void\*>\(0x([A-F0-9]+)\),0x([A-F0-9]+),"([a-f0-9]+)"\}',plan)]
assert len(blocks) == 3
expected_calls = [(int(b),int(o,16),int(t,16)) for b,o,t in re.findall(r'\{([012]),0x([A-F0-9]+),0x([A-F0-9]+)\}',plan)]
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
md.detail = True
calls = []
for block,(start,size,sha) in enumerate(blocks):
    data = read(start,size)
    assert hashlib.sha256(data).hexdigest() == sha
    instructions = list(md.disasm(data,start))
    assert sum(i.size for i in instructions) == size
    boundaries = {i.address for i in instructions}
    for ins in instructions:
        if ins.group(capstone.CS_GRP_JUMP):
            assert ins.operands[0].type == capstone.x86.X86_OP_IMM, (hex(ins.address),'indirect jump')
            assert ins.operands[0].imm in boundaries, (hex(ins.address),'external/interior branch')
        if ins.mnemonic == 'call' and ins.operands[0].type == capstone.x86.X86_OP_IMM:
            assert ins.bytes[0] == 0xe8 and ins.size == 5
            calls.append((block,ins.address-start,ins.operands[0].imm))
assert calls == expected_calls and len(calls) == 155
assert (0,0x3ad,0x5e0d20) in calls  # consumed camera smart ref, ret4
assert (1,0xf85,0x809390) in calls  # native ordinary FP composition
assert read(0x8098fa,5) == bytes.fromhex('807c241700')
assert read(0x8098ff,6) == bytes.fromhex('0f8409010000')  # JE809A0E
assert read(0x73128a,3) == bytes.fromhex('c20400')
assert read(0x7266e4,3) == bytes.fromhex('c21800')
assert read(0x809c02,3) == bytes.fromhex('c20c00')
assert hashlib.sha256(read(0xfdf16c,91*4)).hexdigest() == 'd13a6d5ccdfff5871c8583600a0ac2bcf48cdbda46932c4fa8ee0a460b671f66'
assert read(0xfdf2d8,18) == b'Use Camera Channel'
assert struct.unpack('<I',read(0xfdf170+25*4,4))[0] == 0x730890
print('PASS exact EXE, three complete pinned functions, 155 relocations, internal instruction-boundary branches, ABI, bone branch, 90-slot table boundary')
