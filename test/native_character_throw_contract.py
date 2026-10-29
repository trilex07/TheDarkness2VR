"""Verify the pinned native character velocity consumer; keep game bytes private."""
import argparse
import hashlib
from pathlib import Path
import capstone
import pefile
from native_view_clone_plan import EXE_SHA

parser = argparse.ArgumentParser()
parser.add_argument('--exe', required=True, type=Path)
parser.add_argument('--fixture-body', type=Path)
args = parser.parse_args()
assert hashlib.sha256(args.exe.read_bytes()).hexdigest() == EXE_SHA
pe = pefile.PE(str(args.exe))
decoder = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
contracts = {
    0x8799AF: 'mov esi, dword ptr [edi + 0x584]',
    0x8799BA: 'add dword ptr [esi + 4], eax',
    0x879C98: 'cmp dword ptr [esi], 0',
    0x879C9D: 'push 2',
    0x879C9F: 'lea ecx, [esp + 0x64]',
    0x879CA3: 'push ecx',
    0x879CA4: 'mov ecx, dword ptr [esi]',
    0x879CA6: 'call 0x75ad20',
    0x879DB4: 'dec eax',
    0x879DB7: 'mov dword ptr [esi + 4], eax',
    0x75AD26: 'mov esi, dword ptr [edi + 0x4f0]',
    0x75AD2C: 'cmp esi, dword ptr [edi + 0x4f4]',
    0x75AD34: 'mov ebx, dword ptr [esp + 0x18]',
    0x75AD38: 'mov ebp, dword ptr [esp + 0x14]',
    0x75AD49: 'call 0x8fdda0',
    0x75AD51: 'mov dword ptr [ecx], ebx',
    0x75AD55: 'push ebp',
    0x75AD59: 'mov dword ptr [ecx], edx',
    0x75AD62: 'mov eax, dword ptr [edx + 0xc4]',
    0x75AD68: 'call eax',
    0x75AD79: 'ret 8',
}
for address, expected in contracts.items():
    instruction = next(decoder.disasm(pe.get_data(address - 0x400000, 16), address))
    assert instruction.mnemonic + ' ' + instruction.op_str == expected, hex(address)
body = pe.get_data(0x75AD20 - 0x400000, 0x5C)
instructions = list(decoder.disasm(body, 0x75AD20))
assert instructions[-1].address == 0x75AD79 and instructions[-1].size == 3
assert sum(instruction.size for instruction in instructions) == len(body)
assert hashlib.sha256(body).hexdigest() == 'ba10ca5ef19dac8753a22dcb96ea72b44a4172d9377f9492f28a3637b8922261'
if args.fixture_body:
    output = args.fixture_body.resolve()
    assert not output.is_relative_to(Path(__file__).resolve().parents[1]), 'Keep original bytes outside Git'
    output.write_bytes(body)
print('PASS native character throw: retained ESI target, shared velocity/mode2, body fanout and reference lifetime; 21 instruction contracts plus full consumer SHA256')
