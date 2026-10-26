"""Summarize the game PresentMon rows from one bounded ETW capture.

These are game presents, not PSVR2 display FPS. The ETL and SteamVR timing
remain necessary to identify blocking stacks and compositor/reprojection.
"""
from __future__ import annotations

import csv
import json
import math
import statistics
import sys
from pathlib import Path


FIELDS = ("FrameTime", "CPUBusy", "CPUWait", "GPUBusy", "GPUWait", "GPUTime", "GPULatency")


def percentile(values: list[float], percent: float) -> float:
    ordered = sorted(values)
    if not ordered:
        return math.nan
    index = (len(ordered) - 1) * percent / 100
    lower = math.floor(index)
    upper = math.ceil(index)
    return ordered[lower] + (ordered[upper] - ordered[lower]) * (index - lower)


def summarize(rows: list[dict[str, str]]) -> dict:
    output: dict = {"rows": len(rows), "metrics_ms": {}}
    for key in FIELDS:
        values = []
        for row in rows:
            try:
                value = float(row[key])
            except (ValueError, KeyError, TypeError):
                continue
            if math.isfinite(value) and value >= 0:
                values.append(value)
        output["metrics_ms"][key] = {
            "n": len(values), "mean": statistics.fmean(values) if values else None,
            "p50": percentile(values, 50) if values else None,
            "p95": percentile(values, 95) if values else None,
            "p99": percentile(values, 99) if values else None,
            "worst": max(values) if values else None,
        }
    output["displayed_time_available"] = sum(
        row.get("DisplayedTime", "NA").upper() != "NA" for row in rows
    )
    times = []
    for row in rows:
        try:
            value = float(row['FrameTime'])
            if math.isfinite(value) and value >= 0:
                times.append(value)
        except (KeyError, ValueError, TypeError):
            pass
    output['frames_over_ms'] = {str(t): sum(v > t for v in times) for t in (11.11, 22.22, 33.33)}
    return output


def analyze(directory: Path) -> dict:
    manifest = json.loads((directory / "manifest.json").read_text(encoding="utf-8-sig"))
    with (directory / "presentmon.csv").open(newline="", encoding="utf-8-sig") as source:
        rows = list(csv.DictReader(source))
    game_pid = str(manifest["processIds"]["game"])
    rows = [row for row in rows if row.get("ProcessID") == game_pid]
    frequency = manifest["qpcFrequency"]
    base_start = manifest["baselineQpc"] * 1000 / frequency
    combat_start = manifest["combatQpc"] * 1000 / frequency
    end = manifest["endQpc"] * 1000 / frequency
    def stamp(row: dict[str, str]) -> float:
        try:
            return float(row["CPUStartQPCTime"])
        except (KeyError, ValueError):
            return math.nan
    baseline = [row for row in rows if base_start <= stamp(row) < combat_start]
    combat = [row for row in rows if combat_start <= stamp(row) <= end]
    phases = {}
    worst_outdoor = []
    for phase in manifest.get('phases', []):
        if not phase.get('endQpc') or phase['endQpc'] <= phase['startQpc']:
            raise ValueError(f"Incomplete phase: {phase['id']}")
        start_ms = phase['startQpc'] * 1000 / frequency
        end_ms = phase['endQpc'] * 1000 / frequency
        selected = []
        for row in rows:
            try:
                frame_ms = float(row['FrameTime'])
            except (ValueError, KeyError, TypeError):
                continue
            # Exclude marker/cue gaps and frames crossing a phase edge. These
            # are application Present intervals, not unique XR image counts.
            if math.isfinite(frame_ms) and frame_ms >= 0 and start_ms <= stamp(row) and stamp(row)+frame_ms <= end_ms:
                selected.append(row)
        phases[phase['id']] = summarize(selected)
        if phase['id'] in ('baseline', 'indoor'):
            baseline = selected
        if phase['id'] == 'combat':
            combat = selected
        if phase['id'] in ('outdoor', 'combat'):
            for row in selected:
                worst_outdoor.append({
                    'phase': phase['id'], 'start_qpc': round(stamp(row)*frequency/1000),
                    'end_qpc': round((stamp(row)+float(row['FrameTime']))*frequency/1000),
                    'frame_ms': float(row['FrameTime']),
                    'swapchain': row.get('SwapChainAddress'),
                    'metrics': {k: row.get(k) for k in FIELDS},
                })
    worst_outdoor.sort(key=lambda r: r['frame_ms'], reverse=True)
    return {
        "capture_valid": manifest["valid"],
        "warning": "Game PresentMon timings are not headset display FPS or a thread wait stack.",
        "game_process_id": game_pid,
        "rows_total": len(rows),
        "phases": phases,
        "worst_ten_outdoor_frames": worst_outdoor[:10],
        "baseline": summarize(baseline),
        "combat": summarize(combat),
        "etw_lost_events": manifest["etwLostEvents"],
        "presentmon_lost_events": manifest["presentmonLostEvents"],
    }


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: python analyze_presentmon.py <capture-directory>", file=sys.stderr)
        return 2
    result = analyze(Path(sys.argv[1]))
    print(json.dumps(result, indent=2))
    return 0 if result["capture_valid"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
