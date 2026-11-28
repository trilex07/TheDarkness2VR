"""Audit native targeting ABI/CFG; distribute only metadata, never game bytes."""
import argparse
import hashlib
import struct
from pathlib import Path
import capstone
import pefile
import native_view_clone_plan as cfg

parser = argparse.ArgumentParser()
parser.add_argument('--exe', required=True, type=Path)
parser.add_argument('--header', type=Path)
parser.add_argument('--check-header', type=Path)
args = parser.parse_args()
assert hashlib.sha256(args.exe.read_bytes()).hexdigest() == cfg.EXE_SHA
pe = pefile.PE(str(args.exe))
read = lambda address, size: pe.get_data(address - 0x400000, size)
cfg.FUNCTIONS = [(0xCB0040, 0x18C1)]
block = cfg.audit(pe, *cfg.FUNCTIONS[0])
assert block['sha256'] == '31561db14cd1d134a743f1b2777043ba39db984f05281196e7ed2a6e4501eb37'
assert len(block['calls']) == 124
decoder = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
contracts = {
    0x878B35: 'call 0xcb0040',
    0x878B3A: 'push eax',
    0x878B3F: 'call 0x84bba0',
    0x84BBA5: 'mov esi, dword ptr [eax]',
    0x84BBA7: 'inc dword ptr [esi + 4]',
    0x878D07: 'lea esi, [esp + 0xac]',
    0x878D0E: 'lea ecx, [esp + 0x34]',
    0x878D12: 'call 0xe0bca0',
    0x878D17: 'add esp, 0xc',
    0x878D1A: 'test al, al',
    0xCB01F1: 'ret 0x1c',
    0xCB0351: 'call 0x4bf720',
    0xCB0534: 'call 0x7be900',
    0x7BEA70: 'ret 0x18',
    0x64FDAC: 'movaps xmm1, xmmword ptr [eax + 0x20]',
    0xBF2769: 'movaps xmm0, xmmword ptr [eax + 0x30]',
    0xCB0300: 'movaps xmm0, xmmword ptr [eax + 0x20]',
    0xCB09FB: 'movss dword ptr [eax], xmm1',
    0xCB09FF: 'movaps xmmword ptr [eax + 0x10], xmm0',
    0xCB0A03: 'mov dword ptr [eax + 0x20], esi',
    0xCB0A13: 'add eax, 0x30',
    0xCB0FC5: 'call 0x4e4320',
    0xCB17B1: 'mov eax, dword ptr [esi + 0x20]',
    0xCB17B8: 'mov dword ptr [esi], eax',
    0xAEDC51: 'comiss xmm0, xmm1',
    0xAEDC54: 'movss dword ptr [esp + 0x20], xmm1',
    0xEC5AC5: 'push 0xf9b608',
    0xEC5ACF: 'mov ecx, 0x1110508',
}
for address, expected in contracts.items():
    instruction = next(decoder.disasm(read(address, 16), address))
    assert instruction.mnemonic + ' ' + instruction.op_str == expected, hex(address)
assert read(0xF9B608, 12) == b'D2NpcAvatar\0'
assert abs(struct.unpack('<f', read(0xFABB3C, 4))[0] - 0.01745329252) < 1e-9
text = ['#pragma once', '// Native target selector: CFG audited, SHA-pinned, relocation metadata only.',
        '#include "native_view_code_clone.h"', 'namespace d2vr_native_clone {',
        'inline const Block targetBlocks[]={',
        '    {reinterpret_cast<void*>(0xCB0040),0x18C1,"' + block['sha256'] + '"},',
        '};', 'inline const Call targetCalls[]={']
text += ['    {0,0x%X,0x%X},' % pair for pair in block['calls']]
header = '\n'.join(text + ['};', '}', ''])
if args.header:
    args.header.write_text(header, encoding='utf-8')
if args.check_header:
    assert args.check_header.read_text(encoding='utf-8') == header
print('PASS target selector: full EXE/query SHA, 124 calls/closed CFG, %d ABI/layout/type contracts; no runtime claim' % len(contracts))
