"""Compare ETW leaf exposure in fast/slow PresentMon frame quartiles.

Sample weights are nominal sampled milliseconds, NOT recoverable CPU time.
Native ownership must come from a SHA-checked Ghidra export, never nearest VA.
"""
import argparse
import bisect
import collections
import csv
import hashlib
import json
import pathlib
import re


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=pathlib.Path)
    parser.add_argument("--owners", type=pathlib.Path, required=True)
    parser.add_argument("--tid", type=int, required=True)
    parser.add_argument("--pid", type=int, required=True)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    args = parser.parse_args()
    capture = args.capture
    raw = json.loads((capture / "cpu-raw-all.json").read_text())
    if raw["partial"] or raw["exit"] != 0:
        raise ValueError("Incomplete CPU export")
    origin = json.loads((capture / "qpc-origin.json").read_text())
    origin_us = origin["origin_qpc"] * 1e6 / origin["frequency"]
    _, begin, end = next(p for p in raw["phases"] if p[0] == "combat")
    owner_lines = args.owners.read_text().splitlines()
    if owner_lines[-1] != "EVIDENCE COMPLETE":
        raise ValueError("Incomplete native ownership export")
    owners = {int(r["pc"], 16): r["entry"] for r in
              csv.DictReader(owner_lines[:-1])}
    frames = []
    with (capture / "presentmon.csv").open(newline="") as stream:
        for row in csv.DictReader(stream):
            if int(row["ProcessID"]) != args.pid:
                continue
            start = float(row["CPUStartQPCTime"]) * 1000 - origin_us
            dt = float(row["FrameTime"])
            stop = start + dt * 1000
            if begin <= start < stop <= end and 0 < dt <= 100:
                frames.append(dict(start=start, stop=stop, ms=dt,
                                   module=collections.Counter(),
                                   leaf=collections.Counter(),
                                   native=collections.Counter(),
                                   native_caller=collections.Counter(),
                                   native_inclusive=collections.Counter()))
    starts = [f["start"] for f in frames]
    if len(frames) < 100 or starts != sorted(starts):
        raise ValueError("Insufficient or unsorted frame intervals")
    # CSV rounding permits sub-microsecond differences, not overlapping frames.
    if any(a["stop"] - b["start"] > 2 for a, b in zip(frames, frames[1:])):
        raise ValueError("Overlapping PresentMon intervals / multiple swapchains")
    assigned = missed = 0
    for phase, tid, pc, weight, timestamp, stack in raw["samples"]:
        if phase != "combat" or tid != args.tid:
            continue
        index = bisect.bisect_right(starts, timestamp) - 1
        if index < 0 or timestamp >= frames[index]["stop"]:
            missed += weight
            continue
        frame = frames[index]
        frame["module"][pc.split("!", 1)[0]] += weight
        frame["leaf"][pc] += weight
        assigned += weight
        native = re.fullmatch(r"DarknessII.exe!0x([0-9a-fA-F]+)", pc)
        if native:
            entry = owners.get(int(native[1], 16), "UNMAPPED")
            frame["native"][entry if entry != "UNKNOWN" else pc] += weight
        # First native caller is inclusive of library/driver descendants.
        # Assign once per sample; do not sum it with native leaf exposure.
        native_entries = []
        for address in [pc] + stack:
            match = re.fullmatch(r"DarknessII.exe!0x([0-9a-fA-F]+)", address)
            if match:
                entry = owners.get(int(match[1], 16), "UNMAPPED")
                native_entries.append(entry if entry != "UNKNOWN" else address)
        if native_entries:
            frame["native_caller"][native_entries[0]] += weight
            for entry in set(native_entries):
                frame["native_inclusive"][entry] += weight
    ranked = sorted(frames, key=lambda f: f["ms"])
    count = len(ranked) // 4

    def summarize(group):
        out = {"frames": len(group), "mean_frame_ms":
               sum(f["ms"] for f in group) / len(group)}
        for category in ("module", "leaf", "native", "native_caller", "native_inclusive"):
            total = sum((f[category] for f in group), collections.Counter())
            out[category] = {key: value / len(group)
                             for key, value in total.most_common()}
        return out

    fast, slow = summarize(ranked[:count]), summarize(ranked[-count:])
    delta = {}
    for category in ("module", "leaf", "native", "native_caller", "native_inclusive"):
        keys = fast[category].keys() | slow[category].keys()
        delta[category] = dict(sorted(
            ((k, slow[category].get(k, 0) - fast[category].get(k, 0))
             for k in keys), key=lambda item: item[1], reverse=True))
    result = dict(capture=str(capture), tid=args.tid, pid=args.pid,
                  frames=len(frames), assigned_weight=assigned,
                  outside_selected_frames_weight=missed,
                  units="nominal 1ms sample weight/frame; exposure, not savings",
                  warning="historical instrumented build; no current FPS proof",
                  fast=fast, slow=slow, slow_minus_fast=delta)
    result["input_sha256"] = {
        str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in
        [capture / "cpu-raw-all.json", capture / "presentmon.csv",
         capture / "qpc-origin.json", args.owners]}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({"frames": len(frames), "fast_ms": fast["mean_frame_ms"],
                      "slow_ms": slow["mean_frame_ms"],
                      "native_leaf_delta": list(delta["native"].items())[:20],
                      "native_caller_delta": list(delta["native_caller"].items())[:20]},
                     indent=2))


if __name__ == "__main__":
    main()
