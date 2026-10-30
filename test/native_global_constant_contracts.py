"""Audit exact global payload contracts against a private, pinned game image."""
import argparse
import hashlib
from pathlib import Path
import capstone
import pefile

p = argparse.ArgumentParser()
p.add_argument("--exe", type=Path, required=True)
a = p.parse_args()
assert hashlib.sha256(a.exe.read_bytes()).hexdigest() == "d46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1"
pe = pefile.PE(str(a.exe))
read = lambda va, size: pe.get_data(va - 0x400000, size)
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
header = (Path(__file__).parent.parent / "src/native_constant_source.h").read_text().lower()
cases = [
 (0x571670,0x4e52ac82, [(0x572f6c,0x10dd120),(0x572f86,0x10dd6c0),(0x57300e,0x10dea40),(0x5730d9,0x10dea40),(0x573515,0x10dc0c0),(0x5736c2,0x10dd5b0)]),
 (0xad3380,0x180de9d4, [(0xad4c7c,0x10dc260),(0xad4c96,0x10dc440),(0xad4d1e,0x10dea40),(0xad4de9,0x10dea40),(0xad5225,0x10dd440),(0xad53d2,0x10dc8e0)])]
for entry, fingerprint, sites in cases:
    h = 2166136261
    for b in read(entry,0x20e0): h = ((h ^ b)*16777619)&0xffffffff
    assert h == fingerprint
    for ret, source in sites:
        ops = list(md.disasm(read(ret-10,10),ret-10))
        assert [(x.mnemonic,x.op_str) for x in ops[:2]] == [("push","1"),("push",hex(source))]
        assert len(ops)==4 and ops[2].mnemonic=="push" and ops[3].mnemonic=="call"
        assert ops[-1].address+ops[-1].size==ret
        assert hex(ret) in header and hex(source) in header
        sec=pe.get_section_by_rva(source-0x400000)
        assert sec is not None
        assert sec.Characteristics & 0x40000000 and not sec.Characteristics & 0x02000000
        assert source-0x400000+16 <= sec.VirtualAddress+sec.Misc_VirtualSize
print("PASS pinned image, complete builder fingerprints, 12 exact 16-byte global submissions and nondiscardable readable image extents")

# Exact synchronous getter -> submission chain. At runtime the proof also
# requires the returned source to equal CURRENT owner.block + 0x2e0.
helpers=[0x731970,0x7c2c40,0xbcbd00,0x767230,0x4dbe20]
for returns in [(0x572a2e,0x572c40,0x572c64,0x572cb1,0x573353),
                (0xad473e,0xad4950,0xad4974,0xad49c1,0xad5063)]:
    for ret,helper in zip(returns,helpers):
        ops=list(md.disasm(read(ret-31,31),ret-31))
        assert ops[0].mnemonic=="mov" and ops[0].op_str=="ecx, dword ptr [ebx + 0x1c]"
        calls=[i for i in ops if i.mnemonic=="call"]
        assert len(calls)==2 and calls[0].op_str==hex(helper)
        assert calls[-1].address+calls[-1].size==ret
        assert [i.op_str for i in ops if i.mnemonic=="push"]==["1","eax","eax"]
        assert hex(ret) in header
print("PASS 10 exact current-owner scratch helper/submission chains; runtime address equality mandatory")
