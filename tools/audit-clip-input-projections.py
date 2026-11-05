"""Verify the pinned particle data flow against a local private shader archive."""
import argparse
import hashlib
import json
import re
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path, help="directory containing asm/ and shaders/")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    header = (Path(__file__).resolve().parents[1] / "src/screen_projection_input.h").read_text()
    entries = re.findall(r'\{0x([0-9A-F]{8})u,(\d+),"([0-9a-f]{64})"\}', header)
    if not entries:
        raise RuntimeError("No production identities found")
    results = []
    for key, size, digest in entries:
        blob = (args.archive / "shaders" / f"vs-{key}.bin").read_bytes()
        assembly = (args.archive / "asm" / f"vs-{key}.txt").read_text()
        checks = {
            "length": len(blob) == int(size),
            "sha256": hashlib.sha256(blob).hexdigest() == digest,
            "world_c0": bool(re.search(r'WorldViewProjection\s+c0\s+4', assembly)),
            "texture_c4": bool(re.search(r'ScreenTextureProjection\s+c4\s+4', assembly)),
            "clip_position": "add r0, r0, c3" in assembly and "mov o0, r0" in assembly,
            "postclip_texture": bool(re.search(
                r'mul r1, r0.y, c5\s+mad r1, r0.x, c4, r1\s+'
                r'mad r1, r0.z, c6, r1\s+mad o4, r0.w, c7, r1', assembly)),
        }
        results.append({"vs": key, "checks": checks, "pass": all(checks.values())})
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(results, indent=2))
    if not all(row["pass"] for row in results):
        raise RuntimeError("Pinned shader evidence mismatch; inspect output")
    print(f"PASS {len(results)} exact identities and clip-input data flows")


if __name__ == "__main__":
    main()
