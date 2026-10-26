# Bounded Darkness II combat profile

Run `Capture-DarknessCombat.ps1 -Preflight` first. The capture command requires
the audited game EXE already running in loaded, unpaused SteamVR gameplay. It
does not launch or terminate the game, alter SteamVR, or change DLL/INI files.
Use one representative scene: remain stationary for the first ten seconds,
then fight for the remaining 35 seconds. The script places baseline/combat ETW
marks, records CPU context switches and samples with stacks, DxgKrnl GPU events,
and PresentMon rows for the exact game PID. It saves QPC ticks and hashes of
installed files before/after. Default output is
`%LOCALAPPDATA%\Darkness2VR\perf\<timestamp>`; keep raw ETL/CSV private.

```
pwsh -File tools/perf/Capture-DarknessCombat.ps1 -Preflight
pwsh -File tools/perf/Capture-DarknessCombat.ps1 -DurationSec 45 -BaselineSec 10
python tools/perf/analyze_presentmon.py <capture-directory>
```

`manifest.json` is `complete` only when installed hashes are unchanged and
both ETW and PresentMon lost-event counts are verified as zero. UNKNOWN is an
invalid capture and must be inspected; shorten the trace or lower provider
volume before a deliberate retry. Do not use the older
`tools/run-presentmon-capture.ps1` for this purpose: it rewrites game settings,
replaces the DLL, launches Steam and force-kills the game.

Inspect `combat.etl` in WPA: CPU Usage (Precise) for running/waiting/ready
thread durations and wait stacks; CPU Usage (Sampled) for hotspots; GPU
Utilization/GPU Hardware Queue for scheduling and idle gaps. Use ETW marks and
the diagnostic `[PerfClock] frame/qpc/frequency/pid/tid` markers to align
native frame IDs. `threads.txt`, `cpu-samples.txt` and `etw-stats.txt` are
bounded command-line summaries. PresentMon's game `FrameTime` is not headset
FPS; compare SteamVR compositor timing and reprojection separately. Do not add
overlapping CPU/GPU stage durations to invent a critical path.
