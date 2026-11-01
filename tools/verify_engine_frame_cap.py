"""Offline byte/section evidence for the opt-in limiter. Requires pefile."""
import argparse, hashlib, re, struct
from pathlib import Path
import pefile
p=argparse.ArgumentParser(); p.add_argument("exe",type=Path); args=p.parse_args()
data=args.exe.read_bytes(); pe=pefile.PE(data=data)
assert pe.OPTIONAL_HEADER.ImageBase==0x400000 and pe.FILE_HEADER.Machine==0x14c
source=(Path(__file__).resolve().parents[1]/"src/engine_frame_cap.inl").read_text()
for name,va in [("capBranch",0x938ab2),("sleepLoop",0x938b5f),("capQuery",0x798054),("sleepWrap",0x998130)]:
    body=re.search(r"static const BYTE "+name+r"\[\]=\{([^}]+)\}",source).group(1)
    expected=bytes(int(v.strip(),0) for v in body.split(","))
    assert pe.get_data(va-0x400000,len(expected))==expected, name
    print(name,"PASS")
for va,expected in [(0xf8bf18,b"MaxFrameRate\0"),(0xf38ad8,b"Graphics\0"),(0xfc8034,struct.pack("<f",60)),(0x79805f,bytes.fromhex("c70102000000"))]:
    assert pe.get_data(va-0x400000,len(expected))==expected, hex(va)
assert any(s.VirtualAddress<=0xceb094 and 0xceb098<=s.VirtualAddress+s.Misc_VirtualSize and s.Characteristics&0x80000000 and not s.Characteristics&0x20000000 for s in pe.sections)
print("EXE SHA256",hashlib.sha256(data).hexdigest())
print("Offline layout PASS; active runtime wait and FPS effect NOT tested")
