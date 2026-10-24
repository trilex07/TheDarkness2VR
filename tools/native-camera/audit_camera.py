"""Read-only, build-specific camera evidence. Never starts or patches the sample.

Output is a small identity/site manifest, not a decompilation or memory dump.
No dependency on pefile, Capstone, a running game, or guessed object layouts.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path

KNOWN_SHA256 = "55c8d634fcbe2996d8397d15d68ea728c999697f1dd40e8779b5afe8bbb4c339"
BASE, TIMESTAMP, IMAGE_SIZE = 0x400000, 0x4F68A875, 0xDEC000
# These are instruction sites, NOT names of native projection/frustum functions.
SITES = {
    "avatar_lua_getter_dispatch": (0xDC47BC, "8b8270020000ffd0"),
    "avatar_lua_setter_dispatch": (0xDBDC2B, "8b82e400000083c404ffd0"),
    "cosine_consumer_candidate": (0x7BF850, "8b8270020000ffd0d80d08baff00"),
    "positive_fov_test_candidate": (0x76968A, "8b8270020000ffd0d9eedff1ddd8"),
    "scalar_modifier_candidate": (0xB23F7F, "8b90700200008bceffd2d8642408"),
    "engine_present_boundary": (0xA94052, "8b410cffd0"),
}


class PE:
    def __init__(self, blob: bytes):
        self.blob = blob
        if self.read(0, 2) != b"MZ":
            raise ValueError("not MZ")
        pe = self.u32(0x3C)
        if self.read(pe, 4) != b"PE\0\0":
            raise ValueError("not PE")
        self.machine = self.u16(pe + 4)
        count = self.u16(pe + 6)
        self.timestamp = self.u32(pe + 8)
        opt_size = self.u16(pe + 20)
        opt = pe + 24
        if opt_size < 96 or self.u16(opt) != 0x10B:
            raise ValueError("not PE32")
        self.base = self.u32(opt + 28)
        self.image_size = self.u32(opt + 56)
        self.dll_characteristics = self.u16(opt + 70)
        table = opt + opt_size
        self.read(table, count * 40)
        self.sections = []
        for i in range(count):
            section = table + i * 40
            name = self.read(section, 8).rstrip(b"\0").decode("ascii", "replace")
            virtual_size, rva, size, raw = struct.unpack("<IIII", self.read(section + 8, 16))
            self.read(raw, size)
            self.sections.append((name, rva, virtual_size, raw, size))

    def read(self, off: int, size: int) -> bytes:
        if off < 0 or size < 0 or off + size > len(self.blob):
            raise ValueError("truncated/out-of-range PE")
        return self.blob[off:off + size]

    def u16(self, off: int) -> int:
        return struct.unpack("<H", self.read(off, 2))[0]

    def u32(self, off: int) -> int:
        return struct.unpack("<I", self.read(off, 4))[0]

    def at_va(self, va: int, size: int) -> bytes:
        rva = va - self.base
        for _, start, _, raw, raw_size in self.sections:
            if start <= rva and rva + size <= start + raw_size:
                return self.read(raw + rva - start, size)
        raise ValueError("VA is not backed by section bytes")


def audit(blob: bytes) -> dict:
    pe = PE(blob)
    sha = hashlib.sha256(blob).hexdigest()
    identity = (pe.machine == 0x14C and pe.base == BASE and
                pe.timestamp == TIMESTAMP and pe.image_size == IMAGE_SIZE and
                not (pe.dll_characteristics & 0x40) and sha == KNOWN_SHA256)
    sites = []
    for label, (va, pattern) in SITES.items():
        expected = bytes.fromhex(pattern)
        try:
            observed = pe.at_va(va, len(expected)).hex()
        except ValueError:
            observed = None
        sites.append(dict(label=label, va=f"{va:08X}", expected=pattern,
                          observed=observed, match=observed == pattern))
    return dict(sha256=sha, machine=f"{pe.machine:04X}",
                image_base=f"{pe.base:08X}", timestamp=f"{pe.timestamp:08X}",
                image_size=f"{pe.image_size:08X}", known_identity=identity,
                all_sites_match=all(s["match"] for s in sites), sites=sites,
                native_projection_proven=False, native_cpu_frustum_proven=False)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("exe", type=Path)
    args = parser.parse_args()
    try:
        result = audit(args.exe.read_bytes())
    except (OSError, ValueError) as error:
        print(json.dumps({"status": "BLOCKED", "error": str(error)}))
        return 2
    print(json.dumps(result, indent=2))
    return 0 if result["known_identity"] and result["all_sites_match"] else 3


if __name__ == "__main__":
    raise SystemExit(main())
