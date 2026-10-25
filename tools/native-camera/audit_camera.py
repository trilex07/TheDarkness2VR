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
# Exact Steam-produced variant audited on 2026-10-25: valid Valve Authenticode,
# identical complete .text and all instruction fingerprints. Data/resources and
# signature differ. This is an explicit pin, NOT acceptance of arbitrary files
# with matching code. See docs/research/runtime-readiness-20261025.md.
KNOWN_IDENTITIES = {
    KNOWN_SHA256: "original-audited-20261024",
    "d46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1": "steam-valve-signed-20261025",
}
BASE, TIMESTAMP, IMAGE_SIZE = 0x400000, 0x4F68A875, 0xDEC000
# These are instruction sites, NOT names of native projection/frustum functions.
SITES = {
    "avatar_lua_getter_dispatch": (0xDC47BC, "8b8270020000ffd0"),
    "avatar_lua_setter_dispatch": (0xDBDC2B, "8b82e400000083c404ffd0"),
    "cosine_consumer_candidate": (0x7BF850, "8b8270020000ffd0d80d08baff00"),
    "positive_fov_test_candidate": (0x76968A, "8b8270020000ffd0d9eedff1ddd8"),
    "scalar_modifier_candidate": (0xB23F7F, "8b90700200008bceffd2d8642408"),
    "camera_worker_entry": (0x445E40, "558bec83e4f081ec24070000"),
    "camera_projection_call": (0x44609F, "e84cbf5e008d842420010000"),
    "perspective_builder_entry": (0xA31FF0, "558bec83e4f083ec0cf30f10"),
    "matrix_multiply_entry": (0xC02F50, "558bec83e4f083ec308b4510"),
    "camera_worker_dispatch_entry": (0xD41B30, "558bec83e4f083ec148b450c"),
    "corner_bounds_consumer_entry": (0x70CA40, "558bec83e4f081ec04080000"),
    "bounds_dispatch_entry": (0xAB2E80, "558bec83e4f081ec24060000"),
    "camera_like_constructor_entry": (0xA579E0, "5356578bf1e8d6b4b9ffc706"),
    "engine_present_boundary": (0xA94052, "8b410cffd0"),
}


# Hashes cover complete contiguous instruction listings, not game assets.
# They let restored installations reproduce the private Ghidra range comparison.
FUNCTION_RANGES = {
    0x445E40: (3458, "ade3f3532f3bc43b86affd834f6350656c3af80f8a1a6aef30fe02f93d58509c"),
    0xC02F50: (312, "e649984ed36163d7ea5a1fd1f8735c7b031d63b9a0b939af98ddfe61b27dbf3b"),
    0x70CA40: (2114, "ad820f7b66258bc4dee05456b1fca95c396d1c6d6f6cb56ce1f419d08a34afc7"),
    0xA579E0: (669, "827683b848e4d9a81d04fbe4180101c5494960326c3be7b979738a7efc05055f"),
    0xD41B30: (587, "fe731de65b85822443eeabafa03fe40b3781e8d24660c06d2c0bcc82cd76ccac"),
    0xAB2E80: (2593, "26ab0af8abb2f85cb9ed3f536dad034ef0b6fbe1d47841dbb828f73bf0e68054"),
    0xA93D90: (169, "2739651add00764f61994b8db24bd1d92c8891b03362416c9b3707a6f01f64d0"),
    0xA31FF0: (201, "04dbb868c695587ae0dcf92089b19446a37fce51f01a916302e0184d74b9b93f"),
    0xDF53E0: (439, "8a19546954f15a0b399c08ab4d46d96987d66efb90eaed65bc18b966e04b5030"),
    0xDF3B50: (177, "162a787b4a7e71a1a198941c29444221b809ec4a693923a2b1ed0305d7181a64"),
    # Same producer -> plane set -> CPU candidate/triangle rejection. These
    # fingerprints do not identify this receiver as the active player camera.
    0x607CD0: (967, "c1bad01802581d8fd518bcd6de9b395801d0bdf0250114d8f8e29fb2501e574d"),
    0x685050: (133, "bf1da6d5e905decf77ba414b39bf432fad62990733c5bab3ea84c2a52771a0fe"),
    0xBC9230: (567, "aee95314eab9effedef03368d471afe534ab5f703620914ad23188e613a44517"),
    0xC0F500: (226, "5a9f819c598c9ac1639c25288375c74375dcbc081c68553ae6b64a0e4ac03fb6"),
    0x62DD40: (119, "6c8b4df6e38fe5edae837a0c26b1a461fa3af57b26b9ee375f324c90a69d2652"),
    # 93F2F0 is zeroed in the old depot sample! Analyzed anew in the private
    # InstalledCamera20261024 project from the known original 55C8 executable.
    0x93F2F0: (127, "1b56b152a586844ec68bbe6cda8b976a40ed9fd7bec9c94881ec8cee6765d80f"),
    0x48B7F0: (194, "d29f0ed33b8fe7c5bf149174e333fa9ae08a8ef642b213578c3df208ef35c98a"),
    0x8285E0: (122, "cf57d96914fc0f3defe23225f712cff3fc3a6f438f702ba8a87981f868c59ad5"),
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
                not (pe.dll_characteristics & 0x40) and sha in KNOWN_IDENTITIES)
    sites = []
    for label, (va, pattern) in SITES.items():
        expected = bytes.fromhex(pattern)
        try:
            observed = pe.at_va(va, len(expected)).hex()
        except ValueError:
            observed = None
        sites.append(dict(label=label, va=f"{va:08X}", expected=pattern,
                          observed=observed, match=observed == pattern))
    ranges = []
    for va, (length, expected) in FUNCTION_RANGES.items():
        try:
            observed = hashlib.sha256(pe.at_va(va, length)).hexdigest()
        except ValueError:
            observed = None
        ranges.append(dict(va=f"{va:08X}", length=length, sha256=observed, match=observed==expected))
    return dict(sha256=sha, machine=f"{pe.machine:04X}",
                image_base=f"{pe.base:08X}", timestamp=f"{pe.timestamp:08X}",
                image_size=f"{pe.image_size:08X}", known_identity=identity,
                identity_label=KNOWN_IDENTITIES.get(sha) if identity else None,
                all_sites_match=all(s["match"] for s in sites), sites=sites,
                all_function_ranges_match=all(r["match"] for r in ranges), function_ranges=ranges,
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
    return 0 if result["known_identity"] and result["all_sites_match"] and result["all_function_ranges_match"] else 3


if __name__ == "__main__":
    raise SystemExit(main())
