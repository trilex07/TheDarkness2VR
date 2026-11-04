"""Separate steady 3D/theater/video cadence windows; never infer headset FPS.

Cadence windows spanning any theater-mode transition are rejected, as are
initial windows, missing mode identity, or mixed/non-world boundaries. Logs
alone do not prove focus, an unpaused simulation, or a matched combat scene.
"""
import argparse
import hashlib
import json
import math
import re
from pathlib import Path

MODE = re.compile(r"\[CinematicTheater\] mode=(\d+)\b")
CADENCE = re.compile(r"\[XRCadence\] layers=([\d.]+)/s game-boundaries=([\d.]+)/s world-bearing=([\d.]+)/s")


def summarize(text):
    mode = None
    have_previous = False
    transition = False
    rows = []
    for number, line in enumerate(text.splitlines(), 1):
        match = MODE.search(line)
        if match:
            value = int(match[1])
            if value != mode:
                transition = True
            mode = value
        match = CADENCE.search(line)
        if not match:
            continue
        layers, boundaries, world = map(float, match.groups())
        reason = None
        if not have_previous:
            reason = "initial window"
        elif mode not in (0, 1, 2):
            reason = "unknown mode"
        elif transition:
            reason = "mode transition"
        elif not all(math.isfinite(v) and v > 0 for v in (layers, boundaries, world)):
            reason = "non-world window"
        elif max(layers, boundaries, world) - min(layers, boundaries, world) > .25:
            reason = "mixed boundaries"
        rows.append(dict(line=number, mode=mode, layers_per_second=layers,
                         boundaries_per_second=boundaries, world_per_second=world,
                         rejection=reason))
        have_previous = True
        transition = False
    groups = {}
    for value, name in ((0, "normal_3d"), (1, "native_theater"), (2, "video_theater")):
        accepted = [r["world_per_second"] for r in rows if r["mode"] == value and not r["rejection"]]
        groups[name] = dict(windows=len(accepted), minimum=min(accepted) if accepted else None,
                            maximum=max(accepted) if accepted else None,
                            mean=sum(accepted)/len(accepted) if accepted else None)
    return dict(groups=groups, rows=rows,
                limitations="Emulator layer cadence, not physical HMD or simulation FPS. Scene/focus/pause/load require separate evidence; window phase is log-derived.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    raw = args.log.read_bytes()
    result = summarize(raw.decode("utf-8", errors="replace"))
    result["source_sha256"] = hashlib.sha256(raw).hexdigest()
    print(json.dumps(result, indent=2, allow_nan=False))
