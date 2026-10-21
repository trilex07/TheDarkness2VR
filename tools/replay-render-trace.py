#!/usr/bin/env python3
"""Replay render_pipeline_trace.csv through frame-boundary policies + mutations."""
from __future__ import annotations

import argparse
import csv
import random
import sys
from collections import Counter
from pathlib import Path

# Mirror src/frame_boundary.h Policy enum order for offline checks.
PRESENT = {"DevicePresent", "PresentEx", "SwapChainPresent"}
EVENT_MAP = {
    "BeginScene": 0,
    "EndScene": 1,
    "DevicePresent": 2,
    "PresentEx": 3,
    "SwapChainPresent": 4,
    "Reset": 5,
    "ResetEx": 6,
}


class Machine:
    """Minimal Python port of BoundaryMachine (PresentPrimary / TraceOnly / Legacy)."""

    def __init__(self, policy: str):
        self.policy = policy
        self.open_bs = 0
        self.device_present_seen = False
        self.last_end_ms = 0
        self.vr_frames = 0
        self.debounce_ms = 9

    def on_event(self, ev: str, ms: int) -> bool:
        if ev == "BeginScene":
            self.open_bs += 1
            return False
        if ev == "EndScene":
            if self.open_bs > 0:
                self.open_bs -= 1
            return self._end_scene(ms)
        if ev in PRESENT:
            self.device_present_seen = True
            return self._present(ev)
        if ev in ("Reset", "ResetEx"):
            self.open_bs = 0
            self.last_end_ms = 0
            return False
        return False

    def _present(self, tag: str) -> bool:
        if self.policy == "TraceOnly":
            return False
        if self.policy == "PresentPrimary":
            self.vr_frames += 1
            return True
        self.vr_frames += 1
        return True

    def _end_scene(self, ms: int) -> bool:
        if self.policy == "TraceOnly" or self.policy == "PresentPrimary":
            return False
        if self.device_present_seen or self.open_bs != 0:
            return False
        if ms - self.last_end_ms < self.debounce_ms:
            return False
        self.last_end_ms = ms
        self.vr_frames += 1
        return True


def load_trace(path: Path):
    rows = []
    with path.open(newline="", encoding="utf-8", errors="replace") as f:
        for row in csv.DictReader(f):
            rows.append((int(row["ms"]), row["event"]))
    return rows


def replay(rows, policy: str) -> int:
    m = Machine(policy)
    for ms, ev in rows:
        m.on_event(ev, ms)
    return m.vr_frames


def mutate(rows, rng: random.Random):
    out = list(rows)
    choice = rng.randint(0, 5)
    if choice == 0 and out:
        i = rng.randrange(len(out))
        out.insert(i, (out[i][0], "EndScene"))
    elif choice == 1:
        out = [(ms, ev) for ms, ev in out if ev not in PRESENT or rng.random() > 0.05]
    elif choice == 2 and out:
        ms, ev = out[rng.randrange(len(out))]
        out.append((ms + rng.randint(1, 3), ev))
    elif choice == 3:
        burst = []
        t0 = out[0][0] if out else 0
        for i in range(200):
            burst.append((t0 + i, "BeginScene"))
            burst.append((t0 + i, "EndScene"))
        out = burst + out
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("csv", type=Path)
    ap.add_argument("--mutations", type=int, default=500)
    args = ap.parse_args()
    if not args.csv.exists():
        print(f"Missing {args.csv}", file=sys.stderr)
        return 2
    rows = load_trace(args.csv)
    if len(rows) < 10:
        print("Trace too short", file=sys.stderr)
        return 2
    counts = Counter(ev for _, ev in rows)
    print(f"Loaded {len(rows)} events from {args.csv}")
    print("Counts:", dict(counts))
    for pol in ("TraceOnly", "PresentPrimary", "LegacyRc20b"):
        vr = replay(rows, pol)
        presents = sum(counts.get(p, 0) for p in PRESENT)
        print(f"  {pol}: vrFrames={vr} presents={presents}")
    rng = random.Random(42)
    fails = 0
    for i in range(args.mutations):
        mrows = mutate(rows, rng)
        legacy = replay(mrows, "LegacyRc20b")
        primary = replay(mrows, "PresentPrimary")
        if primary and legacy > primary * 8 + 50:
            fails += 1
    print(f"Mutations: {args.mutations}  legacy>>primary anomalies: {fails}")
    return 0 if fails < args.mutations // 2 else 1


if __name__ == "__main__":
    raise SystemExit(main())
