"""Read-only audit of the specific native getter consumer; no game execution."""
import argparse
import hashlib
import json
from pathlib import Path

import capstone
import pefile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    exe_sha = hashlib.sha256(args.exe.read_bytes()).hexdigest()
    assert exe_sha == "d46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1"
    pe = pefile.PE(str(args.exe))
    assert pe.FILE_HEADER.Machine == 0x14C and pe.OPTIONAL_HEADER.ImageBase == 0x400000
    base = pe.OPTIONAL_HEADER.ImageBase
    read = lambda va, count: pe.get_data(va-base, count)
    u32 = lambda va: int.from_bytes(read(va, 4), "little")
    assert read(0x504810, 7) == bytes.fromhex("8d8180000000c3")
    assert u32(0xFFC7C0+0x9C) == 0x504810
    assert u32(0x1001630+0x2F4) == 0xC7C710
    assert read(0xC7C710, 9) == bytes.fromhex("8b812c0600008b00c3")
    for slot in range(71):
        target = u32(0xFFC7C0+slot*4)
        assert any(section.Characteristics & 0x20000000 and
                   section.VirtualAddress <= target-base < section.VirtualAddress+section.Misc_VirtualSize
                   for section in pe.sections)
    assert u32(0xFFC7C0+71*4) == 0
    consumer_sha = hashlib.sha256(read(0xC62F50, 0x2BE)).hexdigest()
    table_sha = hashlib.sha256(read(0xFFC7BC, 72*4)).hexdigest()
    assert consumer_sha == "f0be0d6ab8a610b998561d68ff4f004efdad6709bc455247f93b9064e17705c8"
    assert table_sha == "b359543d54ab6c337fadf529c733d42628acffcdf8dd848a9980c4aa926fae0b"
    decoder = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    code = list(decoder.disasm(read(0xC63009, 0x32), 0xC63009))
    assert [(i.address, i.mnemonic, i.op_str) for i in code[:4]] == [
        (0xC63009, "mov", "eax, dword ptr [edx + 0x9c]"),
        (0xC6300F, "call", "eax"),
        (0xC63011, "movaps", "xmm0, xmmword ptr [eax + 0x20]"),
        (0xC63015, "movaps", "xmm2, xmmword ptr [eax + 0x30]"),
    ]
    # EAX's two pointer reads immediately consume rows2/3, then the native
    # method replaces EAX with ESP. No pointer to our TLS matrix is retained.
    assert [(i.address, i.mnemonic, i.op_str) for i in code[4:8]][-1] == (0xC63024, "mov", "eax, esp")
    host = (Path(__file__).resolve().parents[1] / "src" / "protected_left_aim.inl").read_text()
    assert consumer_sha in host and table_sha in host
    report = {"status": "STATIC_CONTRACT_PASS_NOT_RUNTIME_ACCEPTANCE", "exe_sha256": exe_sha,
              "consumer_sha256": consumer_sha, "table_sha256": table_sha,
              "getter": "00504810", "return_filter": "00C63011", "native_table_entries": 71,
              "lifetime": "two immediate matrix row reads before EAX replacement; native collision ownership unchanged"}
    if args.report:
        args.report.write_text(json.dumps(report, indent=2)+"\n", encoding="utf-8")
    print("PASS pinned native aim getter,71 methods/header,full consumer and immediate TLS-value consumption; no game execution")


if __name__ == "__main__":
    main()
