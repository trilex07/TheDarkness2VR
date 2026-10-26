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
            return float(row["CPUStartTime"])
        except (KeyError, ValueError):
            return math.nan
    baseline = [row for row in rows if base_start <= stamp(row) < combat_start]
    combat = [row for row in rows if combat_start <= stamp(row) <= end]
    return {
        "capture_valid": manifest["valid"],
        "warning": "Game PresentMon timings are not headset display FPS or a thread wait stack.",
        "game_process_id": game_pid,
        "rows_total": len(rows),
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
