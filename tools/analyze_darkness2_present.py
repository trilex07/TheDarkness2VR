#!/usr/bin/env python3
"""Static scan of DarknessII.exe for D3D9 Present vtable call patterns (x86)."""
from __future__ import annotations

import argparse
import re
import struct
from pathlib import Path

IMAGE_BASE = 0x00400000
# IDirect3DDevice9 vtable byte offsets (index * 4)
VT_PRESENT = 17 * 4
VT_ENDSCENE = 42 * 4
VT_SWAPCHAIN_PRESENT = 3 * 4


def va(rva: int) -> int:
    return IMAGE_BASE + rva


def find_call_via_disp(data: bytes, disp: int, label: str):
    hits = []
    # call dword ptr [reg + disp]  FF 90/91/92/93/96/97 disp32
    for reg, prefix in ((0, 0x90), (1, 0x91), (2, 0x92), (3, 0x93), (6, 0x96), (7, 0x97)):
        pat = bytes([0xFF, prefix]) + struct.pack("<I", disp)
        off = 0
        while True:
            i = data.find(pat, off)
            if i < 0:
                break
            hits.append((i, reg, label))
            off = i + 1
    return hits


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("exe", type=Path, nargs="?", default=None)
    args = ap.parse_args()
    exe = args.exe
    if exe is None:
        try:
            import subprocess
            root = Path(__file__).resolve().parents[1]
            ps = root / "tools/lib/Find-Darkness2Game.ps1"
            out = subprocess.check_output(["powershell", "-NoProfile", "-File", str(ps)], text=True)
            for line in out.splitlines():
                if line.strip().startswith("ExePath"):
                    exe = Path(line.split(":", 1)[1].strip())
                    break
        except Exception:
            pass
    if exe is None or not exe.exists():
        print("DarknessII.exe not found")
        return 2
    data = exe.read_bytes()
    text_start = 0x401000 - IMAGE_BASE
    text_end = min(len(data), 0xEEA730 - IMAGE_BASE)
    text = data[text_start:text_end]
    print(f"Scanning {exe} (.text {len(text)} bytes)")
    groups = [
        (VT_PRESENT, "Device::Present@17"),
        (VT_ENDSCENE, "EndScene@42"),
        (VT_SWAPCHAIN_PRESENT, "SwapChain::Present@3"),
        (121 * 4, "PresentEx@121"),
    ]
    for disp, label in groups:
        hits = find_call_via_disp(text, disp, label)
        print(f"\n{label}  disp=0x{disp:X}  hits={len(hits)}")
        for rva, reg, _ in hits[:25]:
            print(f"  VA=0x{va(rva + text_start):08X}  call [reg{reg}+0x{disp:X}]")
        if len(hits) > 25:
            print(f"  ... +{len(hits)-25} more")
    # Direct imports
    for name in (b"Direct3DCreate9", b"Direct3DCreate9Ex", b"d3d9.dll"):
        idx = data.find(name)
        if idx >= 0:
            print(f"\nString {name.decode()} @ file offset 0x{idx:X}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
