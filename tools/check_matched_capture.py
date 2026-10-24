"""Reject incomplete/mixed captures, then optionally invoke Luna's ray comparison.

Source frustum must be independently verified from a world draw's bank. The
native inverse-projection candidate is deliberately NOT an automatic authority.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import subprocess
import sys

FILES = {"manifest.jsonl", "vs-banks.bin", "effective-config.json",
         "source-eye0.bmp", "source-eye1.bmp", "final-eye0.bmp", "final-eye1.bmp"}


def validate(folder):
    folder = Path(folder)
    receipt = json.loads((folder / "COMPLETE.json").read_text())
    if set(receipt) != FILES:
        raise ValueError("incomplete receipt")
    for name, digest in receipt.items():
        if hashlib.sha256((folder / name).read_bytes()).hexdigest() != digest:
            raise ValueError(f"hash mismatch: {name}")
    records = [json.loads(line) for line in (folder / "manifest.jsonl").read_text().splitlines()]
    # Keep draw-order validation linear even for the bounded 32K-draw capture.
    record_positions = {id(record): index for index, record in enumerate(records)}
    def position(record):
        return record_positions[id(record)]
    if not records or records[0]["kind"] != "header" or records[-1]["kind"] != "result":
        raise ValueError("missing header or terminal result")
    frame = records[0]["frame"]
    if any(r.get("frame", frame) != frame for r in records):
        raise ValueError("mixed frame IDs")
    if any(r["kind"] == "rejection" for r in records) or not records[-1]["complete"]:
        raise ValueError("runtime rejected capture")
    entries=[r for r in records if r["kind"]=="present_entry"]
    arms=[r for r in records if r["kind"]=="capture_arm"]
    completions=[r for r in records if r["kind"]=="present_completion"]
    staged=[r for r in records if r["kind"]=="staged_result"]
    if records[0].get("schema")!=2 or len(arms)!=1 or len(entries)!=1 or len(completions)!=1 or len(staged)!=1:
        raise ValueError("missing current Present witness")
    entry, completion=entries[0],completions[0]
    arm=arms[0]
    if (arm.get("expected_serial")!=entry.get("serial") or
        arm.get("expected_game_frame")!=entry.get("game_frame") or
        any(k not in arm or k not in entry or arm[k]!=entry[k] for k in ("epoch","thread","reset_generation","device"))):
        raise ValueError("capture arm does not match closing Present")
    if (entry.get("boundary") not in ("DevicePresent","PresentEx","SwapChainPresent") or
        not isinstance(entry.get("serial"),int) or entry["serial"]<=0 or
        any(entry.get(k)!=completion.get(k) for k in ("serial","epoch","game_frame")) or
        not completion.get("same_witness") or not completion.get("forwarded") or completion.get("hr")!=0):
        raise ValueError("invalid Present completion")
    if not staged[0].get("complete") or not (position(arm)<position(entry)<position(staged[0])<position(completion)==len(records)-2):
        raise ValueError("invalid Present boundary ordering")
    if records[-1]["mask"] != 15:
        raise ValueError("incomplete image mask")
    if any(len(records[0].get(k, "")) != 64 for k in
           ("dll_sha256", "exe_sha256", "ini_sha256", "request_sha256")):
        raise ValueError("missing build/config identity")
    result = {"frame": frame, "eyes": []}
    expected_offset = 0
    for r in records:
        if r["kind"] == "draw":
            if position(r)<position(arm):
                raise ValueError("draw outside armed frame")
            if r["eye"] not in (0, 1) or r["bank_offset"] != expected_offset:
                raise ValueError("invalid draw-bank ordering")
            expected_offset += 4096
    if (folder / "vs-banks.bin").stat().st_size != expected_offset:
        raise ValueError("truncated or extra draw banks")
    for eye in (0, 1):
        by_kind = {}
        for kind in ("source", "input", "final", "blit", "submission"):
            matches = [r for r in records if r["kind"] == kind and r["eye"] == eye]
            if len(matches) != 1:
                raise ValueError(f"missing or duplicate {kind} eye {eye}")
            by_kind[kind] = matches[0]
        draws = [r for r in records if r["kind"] == "draw" and r["eye"] == eye]
        if not draws or len(draws) != records[-1]["draws"][eye]:
            raise ValueError("draw count mismatch")
        if [r["draw"] for r in draws] != list(range(len(draws))):
            raise ValueError("draw sequence mismatch")
        source, final = by_kind["source"], by_kind["final"]
        # Luna reads BMP dimensions too; reject switched/foreign files here.
        for kind, image in (("source", source), ("final", final)):
            import struct
            raw = (folder / f"{kind}-eye{eye}.bmp").read_bytes()
            if raw[:2] != b"BM" or len(raw) < 54:
                raise ValueError("invalid BMP")
            w, h = struct.unpack_from("<ii", raw, 18)
            if (w, abs(h)) != (image["width"], image["height"]):
                raise ValueError("image dimensions disagree with manifest")
            if len(raw) != 54 + w * abs(h) * 4:
                raise ValueError("truncated BMP pixels")
        if source.get("rt") != by_kind["input"].get("rt"):
            raise ValueError("source differs from production upload")
        source_i = position(source)
        if not (position(entry)<source_i and position(by_kind["submission"])<position(staged[0])):
            raise ValueError("images outside Present witness")
        if not (source_i < position(by_kind["input"]) < position(by_kind["blit"]) < position(final) < position(by_kind["submission"])):
            raise ValueError("invalid image/metadata boundary ordering")
        if any(position(d) > source_i for d in draws):
            raise ValueError("source captured before its draws completed")
        if eye == 0 and any(r["kind"] == "draw" and r["eye"] == 1 for r in records[:source_i]):
            raise ValueError("left source captured after replay started")
        result["eyes"].append(by_kind)
    config = json.loads((folder / "effective-config.json").read_text())
    identities = [r for r in records if r["kind"] == "config_identity"]
    if len(identities) != 1 or identities[0]["effective_config_sha256"] != receipt["effective-config.json"]:
        raise ValueError("effective config identity mismatch")
    result["config"] = config
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("folder", type=Path)
    parser.add_argument("--eye", type=int, choices=(0, 1))
    parser.add_argument("--source-fov-tan", type=float, nargs=4)
    parser.add_argument("--camera-proof-note", help="world draw/shader/register and matrix convention verified by reviewer")
    parser.add_argument("--roi", nargs=4, type=int)
    args = parser.parse_args()
    try:
        result = validate(args.folder)
        if args.eye is None:
            print(json.dumps({"frame": result["frame"], "matched_files": True,
                              "projection_ownership_proven": False}))
            return 0
        if not args.source_fov_tan or not args.camera_proof_note:
            raise ValueError("ray comparison requires independently verified source frustum and camera proof note")
        eye = result["eyes"][args.eye]
        source, final, blit = eye["source"], eye["final"], eye["blit"]
        if source["viewport"] != [0, 0, source["width"], source["height"], 0, 1]:
            raise ValueError("Luna full-viewport assumption not satisfied")
        if result["config"]["eye_params"][args.eye]["head_geometry"]:
            raise ValueError("head geometry requires an independently established source/target common pose")
        cb = blit["cb_floats"]
        if len(cb) != 24 or cb[15] != 0 or cb[16] != 0 or cb[17] != 3:
            raise ValueError("unsupported shifted/non-geometric blit")
        if blit["viewport"] != [0, 0, final["width"], final["height"], 0, 1]:
            raise ValueError("unsupported final viewport")
        target = [math.tan(x) for x in eye["submission"]["fov_l_r_u_d"]]
        command = [sys.executable, str(Path(__file__).with_name("clarity_capture_analysis.py")),
                   "ray-compare", str(args.folder / f"source-eye{args.eye}.bmp"),
                   str(args.folder / f"final-eye{args.eye}.bmp"), "--source-fov-tan",
                   *map(str, args.source_fov_tan), "--target-fov-tan", *map(str, target),
                   "--crop-x", str(cb[18]), "--crop-y", str(cb[19]), "--json"]
        if args.roi:
            command.extend(["--roi", *map(str, args.roi)])
        print(json.dumps({"camera_proof_note": args.camera_proof_note}), flush=True)
        return subprocess.run(command, check=False).returncode
    except (OSError, ValueError, KeyError, IndexError, TypeError) as error:
        print(f"REJECT: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
