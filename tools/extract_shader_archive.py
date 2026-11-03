"""Extract bounded DSW1/DSW2 shader archives for local bytecode inspection."""
import argparse
import struct
from pathlib import Path


def extract(source, output):
    data = source.read_bytes()
    if len(data) < 8 or len(data) > 4 * 1024 * 1024:
        raise ValueError("archive size")
    magic, count = struct.unpack_from("<II", data)
    if magic not in (0x31575344, 0x32575344) or not 0 < count <= 4096:
        raise ValueError("archive header")
    pos, entries = 8, []
    for _ in range(count):
        if magic == 0x32575344:
            quota, = struct.unpack_from("<I", data, pos)
            pos += 4
            if not 1 <= quota <= 8:
                raise ValueError("quota")
        words, = struct.unpack_from("<I", data, pos)
        pos += 4
        if not 2 <= words <= 8192 or pos + words * 4 > len(data):
            raise ValueError("shader bounds")
        code = data[pos:pos + words * 4]
        pos += len(code)
        version, = struct.unpack_from("<I", code)
        if version not in (0xfffe0300, 0xffff0300) or code[-4:] != b"\xff\xff\x00\x00":
            raise ValueError("shader version/end")
        h = 2166136261
        for b in code:
            h = ((h ^ b) * 16777619) & 0xffffffff
        entries.append((f"{'ps' if version == 0xffff0300 else 'vs'}-{h:08X}.bin", code))
    if pos != len(data):
        raise ValueError("trailing archive bytes")
    output.mkdir(parents=True, exist_ok=True)
    for name, code in entries:
        target = output / name
        if target.exists() and target.read_bytes() != code:
            raise ValueError("hash collision")
        target.write_bytes(code)
    return len(entries)


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("source", type=Path)
    p.add_argument("output", type=Path)
    args = p.parse_args()
    print(extract(args.source, args.output))
