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

## One protected indoor-to-outdoor recording

Use the existing collector with `-OutdoorTransition -AudioCues -SymbolSource
<exact-build-directory>`. Default outdoor plan is 60 seconds: one beep = remain
indoors for 10s; two = exit the building during 15s; three = remain stationary
outdoors for 10s; four = move/fight outdoors for 25s; five = recording ended.
Do not open pause or remove the HMD until the end signal. A phase is an
instruction, not proof of the scene: verify actual focus/world and user actions.
Marker/cue gaps remain in the raw ETL/CSV; they are excluded only from strict
phase summaries, not discarded as possible transition stutters.

CPU.Verbose + GPU.Light remain system-wide. Outdoor PresentMon is also
all-process so compositor/DWM data is retained. No renderer, resolution,
synchronization, resource or native hook changes are made by this collector.
No per-draw timers or synchronous per-frame log writes are introduced. Every
WPR mark has QPC-before/after bounds; phase active intervals use their own QPC
start/end. The analyzer rejects crossing frames and exports the ten worst
outdoor Present intervals for precise CPU/GPU timeline joins.

Freeze DLL, matching PDB and MAP from the same link before any subsequent
build. Outdoor mode requires all three and verifies the symbol-source DLL
against the installed measurement DLL, then copies and SHA-checks them before
capture. PDB GUID/age must be independently checked against DLL RSDS before
preparing the frozen package; matching a filename alone is not sufficient.
WPR supplies image load/base information, CPU execution/scheduling and stack
samples; private driver symbols and missing OpenXR stacks must remain UNKNOWN.
PresentMon CPUBusy/CPUWait are not CPU Precise running/blocked/ready partitions.
Current protected outdoor cause cannot be inferred from the old EXE-patching
093553 or log-A/B traces, whose scene/transition identity was not recorded.

Offline checks (no capture, no Steam):

```
pwsh -File tools/perf/test_capture_outdoor.ps1
python -m unittest discover -s tools/perf -p test_analyze_presentmon.py
```

For the isolated `AsyncLog` A/B, preserve both `d3d9.log` and
`d3d9-emergency.log`. The collector records SHA-256 of each copied log
snapshot. Fatal first-chance records use the emergency sink; ordinary queued
lines can be lost at process exit. `[AsyncLog] NOT_ADMITTED`, missing active
receipt, queue losses or writer errors invalidate a clean performance claim.
Enable the option in the effective base `[Render]` INI, not only an override.


PresentMon 2.6 uses `CPUStartQPCTimeInMs`; the collector accepts that explicit
millisecond column and canonical `CPUStartQPCTime`. It rejects tick-only,
relative, missing and nonfinite values when assigning phase rows. This matches
the retained Python analyzer and does not alter capture providers or game state.

### Lightweight transfer-worker comparisons

Use the existing collector with `-PresentMonOnly` for mode comparisons that do
not require stack attribution. It retains the same audited PID/start time,
QPC phase schedule, DLL/INI/symbol identities, PresentMon loss query and log
snapshots. It starts no WPR session and sends no WPR markers.
`collector=PresentMonOnly`, `stacksCaptured=false`, `etl=null` and
`etwLostEvents=null` explicitly distinguish this evidence from CPU-Precise
traces. Validity still requires known zero PresentMon event/buffer loss and
complete phase rows. Do not infer RUN/READY/WAIT or headset display FPS from it.
