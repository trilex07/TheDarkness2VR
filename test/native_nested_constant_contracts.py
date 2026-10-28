"""Verify pinned native nested-field submissions; never export EXE bytes."""
import argparse
import hashlib
from pathlib import Path

import capstone as cs
import pefile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", required=True, type=Path)
    args = parser.parse_args()
    assert hashlib.sha256(args.exe.read_bytes()).hexdigest() == (
        "d46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1"
    )
    pe = pefile.PE(str(args.exe))
    read = lambda va, count: pe.get_data(va - 0x400000, count)
    decoder = cs.Cs(cs.CS_ARCH_X86, cs.CS_MODE_32)
    decoder.detail = True
    headers = (Path(__file__).parent.parent / "src/native_constant_source.h").read_text()
    cases = [
        (0x571670, 0x4E52AC82, 0xC,
         [(0x571D74, 0x571DB2), (0x571DB7, 0x571DEA),
          (0x571DEF, 0x571E1F), (0x57237B, 0x5723A1)]),
        (0xAD3380, 0x180DE9D4, 0x14,
         [(0xAD3A84, 0xAD3AC2), (0xAD3AC7, 0xAD3AFA),
          (0xAD3AFF, 0xAD3B2F), (0xAD408B, 0xAD40B1)]),
    ]
    for entry, expected_hash, method, sites in cases:
        checksum = 2166136261
        for value in read(entry, 0x20E0):
            checksum = ((checksum ^ value) * 16777619) & 0xFFFFFFFF
        assert checksum == expected_hash, hex(entry)
        for field, (start, end) in enumerate(sites):
            instructions = list(decoder.disasm(read(start, end - start), start))
            assert instructions[-1].mnemonic == "call"
            assert instructions[-1].address + instructions[-1].size == end
            # Match every pointer-producing instruction in this branch. x87
            # pops, owner null tests and their jumps do not produce the payload.
            pointer_ops = [f"{i.mnemonic} {i.op_str}" for i in instructions
                           if i.mnemonic in ("mov", "add")]
            if field == 3:
                expected = ["mov edx, dword ptr [ebx + 0x1c]",
                            "mov edx, dword ptr [edx + 0x5d8]",
                            "mov ecx, dword ptr [edi]", "mov eax, dword ptr [ecx]",
                            f"mov eax, dword ptr [eax + {hex(method)}]", "add edx, 0xc0"]
            else:
                expected = ["mov eax, dword ptr [ebx + 0x1c]",
                            "mov eax, dword ptr [eax + 0x5d4]",
                            "mov eax, dword ptr [eax + 0x154]", "mov eax, dword ptr [eax]",
                            "mov ecx, dword ptr [edi]", "mov edx, dword ptr [ecx]",
                            f"mov edx, dword ptr [edx + {hex(method)}]"]
                if field < 2:
                    expected += [f"add eax, {hex((0xe0, 0x70)[field])}"]
            assert pointer_ops == expected, (hex(start), pointer_ops)
            pushes = [i.op_str for i in instructions if i.mnemonic == "push"]
            vectors = (6, 1, 9, 4)[field]
            assert pushes == [str(vectors), "edx" if field == 3 else "eax",
                              "edx" if field == 3 else "eax"], pushes
            assert hex(end) in headers.lower(), hex(end)
    print("PASS pinned EXE/full builder fingerprints and 8 VS/PS nested-field pointer/count/call contracts")


if __name__ == "__main__":
    main()
