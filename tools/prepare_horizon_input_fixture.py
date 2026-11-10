"""Export pinned local input bodies for an x86 CPU child, never a game process.

Generated native bytes/disassembly stay ignored. No hooks, writes or live reads.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path
import capstone

METHODS = [
    ('axis', 0x95ED30, 0x19C, 'd27b1a89e91d7882da6a363b197cbe2752e312293c81f2dbf125bcfc0409c577'),
    ('store', 0x93A950, 0x1A, 'b71f68f2b24c41717f23d3118c35d8551a27930cc46e3b7b9aeb4f24c1637c6e'),
    ('tick', 0x458100, 0x36, '30cafca5d516df2b727241fb8146ce110e74570926c65144eb1c9df375b3e253'),
    ('reset', 0x9DBE80, 0x34, 'dc89ee664206cbe1290c9bc618c4f463c98f97082e506e92c05981b22998f4e8'),
    ('event', 0x93E990, 0x4B, '6ff817f2fc1e03ff71ad5850a1e738edf31d6c3bed014eacc57286a2f490d0f5'),
    ('poll', 0x89B9E0, 0x14B, '61312d3a06416e296be7d94cd5a45efe0bedef2db65e2b52c0ccb7ddea9c9726'),
]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--exe', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    a = p.parse_args()
    d = a.exe.read_bytes()
    pe = struct.unpack_from('<I', d, 60)[0]
    opt = pe + 24
    st = opt + struct.unpack_from('<H', d, pe + 20)[0]
    base = struct.unpack_from('<I', d, opt + 28)[0]
    sections = [struct.unpack_from('<IIII', d, st + 40*i + 8)
                for i in range(struct.unpack_from('<H', d, pe + 6)[0])]

    def read(va, n):
        for _, rva, size, raw in sections:
            if rva <= va-base and va-base+n <= rva+size:
                return d[raw+va-base-rva:raw+va-base-rva+n]
        raise ValueError('unmapped ' + hex(va))

    a.out.mkdir(parents=True, exist_ok=True)
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    bodies = {}
    for name, va, n, digest in METHODS:
        b = read(va, n)
        assert hashlib.sha256(b).hexdigest() == digest, name
        ins = list(md.disasm(b, va))
        assert sum(i.size for i in ins) == n and ins[-1].mnemonic == 'ret', name+' full boundary'
        changed = bytearray(b)
        changed[n//2] ^= 1
        assert hashlib.sha256(changed).hexdigest() != digest
        bodies[name] = b
        (a.out/(name+'.bin')).write_bytes(b)

    # Actual analog body has no calls. Relocate only admitted constant loads.
    # The old static audit's 0x19A slice omitted two bytes of final RET14;
    # this fixture pins the COMPLETE 0x19C body, correcting that prior label.
    allowed = {0xF9C2A4, 0xF8EB00, 0xFBF738, 0xF72094, 0xFAB780}
    reloc, constants = [], bytearray()
    addresses = {}
    for i in md.disasm(bodies['axis'], 0x95ED30):
        assert i.mnemonic != 'call'
        for op in i.operands:
            if op.type == capstone.x86.X86_OP_MEM and not op.mem.base and not op.mem.index:
                address = op.mem.disp
                assert address in allowed and i.disp_size == 4
                if address not in addresses:
                    addresses[address] = len(constants)
                    constants.extend(read(address, 16))
                reloc.append((i.address-0x95ED30+i.disp_offset, addresses[address]))
    assert set(addresses) == allowed
    assert struct.unpack('<f', read(0xF9C2A4, 4))[0] == -1
    assert struct.unpack('<f', read(0xF72094, 4))[0] == 1
    assert struct.unpack('<I', read(0xF8EB00, 4))[0] == 0x7FFFFFFF
    assert struct.unpack('<I', read(0xFAB780, 4))[0] == 0x80000000
    (a.out/'constants.bin').write_bytes(constants)
    (a.out/'horizon_relocations.h').write_text(
        'struct Relocation {unsigned offset,constant;};\nstatic Relocation relocations[]={'+
        ','.join('{%d,%d}' % x for x in reloc)+'};\n')
    poll_reloc = []
    for i in md.disasm(bodies['poll'], 0x89B9E0):
        if i.mnemonic == 'call' and i.operands[0].type == capstone.x86.X86_OP_IMM:
            target = i.operands[0].imm
            assert target in (0x89AF20, 0x4DA250, 0x874790, 0x47DB10, 0xE441D2, 0x55FC60)
            poll_reloc.append((i.address-0x89B9E0+1, target))
    assert len(poll_reloc) == 7
    with (a.out/'horizon_relocations.h').open('a') as stream:
        stream.write('struct PollRelocation {unsigned offset,target;};\nstatic PollRelocation pollRelocations[]={'+
                     ','.join('{%d,0x%x}' % x for x in poll_reloc)+'};\n')
    root = Path(__file__).resolve().parents[1]
    source = (root/'src/d3d9.cpp').read_text()
    start = source.index('static void FinalizePadPacket(')
    end = source.index('DWORD WINAPI Hook_XInputGetState(', start)
    (a.out/'actual_packet_finalizer.inl').write_text(source[start:end])
    hook = source[end:source.index('static bool HookDelayIATOrdinal(', end)]
    assert hook.count('FinalizePadPacket(userIndex,ERROR_SUCCESS,state);') == 1
    assert hook.count('FinalizePadPacket(userIndex,r,state);') == 1
    assert hook.index('state->Gamepad.sThumbRY = (SHORT)') < hook.index('FinalizePadPacket(userIndex,ERROR_SUCCESS,state);')
    select_start = hook.index('    DWORD res = ERROR_DEVICE_NOT_CONNECTED;')
    select_end = hook.index('    if (res == ERROR_SUCCESS)', select_start)
    predicate_start = source.index('static bool PadHasInput(')
    predicate_end = source.index('void UpdateCachedXrPad()', predicate_start)
    actual_selector = source[predicate_start:predicate_end] + '\nstatic DWORD ActualSelect(DWORD userIndex,const XINPUT_STATE& real,DWORD r,XINPUT_STATE* state) {\n' + hook[select_start:select_end] + '\nreturn res;\n}\n'
    (a.out/'actual_packet_selector.inl').write_text(actual_selector)

    for call, target in [(0x93E9A3, 0x9DBE80), (0x93E9B8, 0x93A950)]:
        b = read(call, 5)
        assert b[0] == 0xE8 and call+5+struct.unpack_from('<i', b, 1)[0] == target
    slots = [(0xFDDAD0+0xF0, 0xC85270), (0xFE3EE8+0x9C, 0x5322B0),
             (0x1001630+0x2F4, 0xC7C710), (0xFFC7C0+0x68, 0xB59D50)]
    for slot, target in slots:
        assert struct.unpack('<I', read(slot, 4))[0] == target
    receipt = dict(exe_sha256=hashlib.sha256(d).hexdigest(), methods=METHODS,
                   slots=slots, axis_constant_relocations=reloc,poll_call_relocations=poll_reloc,
                   finalizer_source_sha256=hashlib.sha256(source[start:end].encode()).hexdigest(),
                   selector_source_sha256=hashlib.sha256(actual_selector.encode()).hexdigest(),
                   scope='CPU typed analog/store/tick/reset/event + actual native poller. External API/raw builder callbacks are fakes; builder invokes actual analog/event bodies. Fake descriptors/null downstream owner. No native camera integration, actual mapping profile, game, XR, GPU or installation.')
    (a.out/'inputs.json').write_text(json.dumps(receipt, indent=2))
    print('PASS 6 full methods/mutations, 4 owner slots, complete RET boundaries; CPU fixture export')


if __name__ == '__main__':
    main()
