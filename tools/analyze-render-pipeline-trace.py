#!/usr/bin/env python3
"""Analyze render_pipeline_trace.csv from Darkness2VR research builds."""
from __future__ import annotations

import argparse
import csv
import statistics
import sys
from collections import Counter, defaultdict
from pathlib import Path

PRESENT_EVENTS = {"DevicePresent", "PresentEx", "SwapChainPresent"}


def load_rows(path: Path):
    with path.open(newline="", encoding="utf-8", errors="replace") as f:
        reader = csv.DictReader(f)
        for row in reader:
            try:
                row["ms"] = int(row["ms"])
                row["draws"] = int(row.get("draws") or 0)
                row["records"] = int(row.get("records") or 0)
            except ValueError:
                continue
            yield row


def rate(events, t0: int, t1: int) -> float:
    if t1 <= t0:
        return 0.0
    return 1000.0 * len(events) / (t1 - t0)


def segment_phases(rows):
    """Heuristic phase split by event density."""
    if not rows:
        return []
    t_min, t_max = rows[0]["ms"], rows[-1]["ms"]
    window = 5000
    segments = []
    start = t_min
    while start < t_max:
        end = start + window
        chunk = [r for r in rows if start <= r["ms"] < end]
        if chunk:
            segments.append((start, end, chunk))
        start = end
    return segments


def endscene_per_present(chunk):
    presents = [r for r in chunk if r["event"] in PRESENT_EVENTS]
    if not presents:
        return None
    es = sum(1 for r in chunk if r["event"] == "EndScene")
    return es / len(presents)


def summarize_chunk(label: str, chunk):
    if not chunk:
        return
    t0, t1 = chunk[0]["ms"], chunk[-1]["ms"]
    counts = Counter(r["event"] for r in chunk)
    draws = [r["draws"] for r in chunk if r["event"] == "EndScene"]
    recs = [r["records"] for r in chunk if r["event"] == "EndScene"]
    ratio = endscene_per_present(chunk)
    print(f"\n### {label}  ({t0}–{t1} ms, {len(chunk)} events)")
    for ev in ("BeginScene", "EndScene", "DevicePresent", "PresentEx", "SwapChainPresent", "SetRenderTarget"):
        c = counts.get(ev, 0)
        print(f"  {ev}: {rate([1]*c, t0, t1):.2f}/s  (count={c})")
    if ratio is not None:
        print(f"  EndScene per Present (mean): {ratio:.2f}")
    if draws:
        print(f"  draws/EndScene p50={statistics.median(draws):.0f} p95={sorted(draws)[int(len(draws)*0.95)-1]}")
    if recs:
        print(f"  records/EndScene p50={statistics.median(recs):.0f} p95={sorted(recs)[int(len(recs)*0.95)-1]}")


def summarize_present_frames(rows):
    """Metrics between consecutive SwapChainPresent events."""
    idx = [i for i, r in enumerate(rows) if r["event"] == "SwapChainPresent"]
    if len(idx) < 2:
        print("\n### SwapChain-presented frames")
        print(f"  frames={len(idx)} (need >=2 for rates)")
        return
    print("\n### SwapChain-presented frames")
    gaps = []
    es_ratios = []
    for a, b in zip(idx, idx[1:]):
        chunk = rows[a : b + 1]
        t0, t1 = chunk[0]["ms"], chunk[-1]["ms"]
        gap = t1 - t0
        gaps.append(gap)
        es = sum(1 for r in chunk if r["event"] == "EndScene")
        es_ratios.append(es)
    print(f"  frames={len(idx)} median_gap_ms={statistics.median(gaps):.2f} p95={sorted(gaps)[int(len(gaps)*0.95)-1]:.2f}")
    print(f"  EndScene/frame p50={statistics.median(es_ratios):.1f} p95={sorted(es_ratios)[int(len(es_ratios)*0.95)-1]}")


def cluster_present_gaps(rows):
    pts = [r["ms"] for r in rows if r["event"] in PRESENT_EVENTS]
    if len(pts) < 2:
        return
    gaps = [b - a for a, b in zip(pts, pts[1:])]
    print("\n### Present timing (primary path)")
    print(f"  samples={len(gaps)} median_gap_ms={statistics.median(gaps):.2f} p95={sorted(gaps)[int(len(gaps)*0.95)-1]:.2f}")


def retaddr_top(path: Path, limit=20):
    ret_path = path.parent / "render_pipeline_retaddr.csv"
    if not ret_path.exists():
        return
    counts = []
    with ret_path.open(newline="", encoding="utf-8", errors="replace") as f:
        for row in csv.DictReader(f):
            try:
                counts.append((row["retaddr"], int(row["count"])))
            except (KeyError, ValueError):
                pass
    counts.sort(key=lambda x: -x[1])
    print("\n### Top return addresses")
    for addr, c in counts[:limit]:
        print(f"  {addr}  count={c}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("csv", type=Path, nargs="?", default=None)
    args = ap.parse_args()
    default = Path.home() / "AppData/Local/Darkness2VR/render_pipeline_trace.csv"
    path = args.csv or default
    if not path.exists():
        print(f"Missing trace: {path}", file=sys.stderr)
        return 2
    rows = list(load_rows(path))
    print(f"Trace: {path}  events={len(rows)}")
    devices = Counter(r.get("device") for r in rows)
    print(f"Unique device pointers: {len(devices)}")
    for ev in PRESENT_EVENTS:
        print(f"  {ev} total={sum(1 for r in rows if r['event']==ev)}")
    cluster_present_gaps(rows)
    summarize_present_frames(rows)
    segs = segment_phases(rows)
    for i, (a, b, chunk) in enumerate(segs):
        summarize_chunk(f"window_{i}_{a}_{b}ms", chunk)
    retaddr_top(path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
