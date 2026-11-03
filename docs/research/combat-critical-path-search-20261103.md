# Combat critical-path search, 2026-11-03

## Result

No multi-millisecond independent main-thread computation has been established.
Do not build another worker from a convenient ownership boundary alone. The
retained full-packet experiment remains OFF and is not a major-FPS candidate.
This investigation changes analysis tooling/documentation only; no runtime code,
Steam deployment, resolution change, game launch, or headset claim.

The new evidence is frame-correlated attribution, rather than aggregate hot
functions: slower combat frames grow across native submission, proxy/runtime,
copies and synchronization. Native job computation is already parallel.

## Inputs and attribution

- Complete capture: `%LOCALAPPDATA%/Darkness2VR/perf/20261030T221816Z-61b77f88`.
  PID26912, render/main TID7828. Combat interval7778680.1..32798174.3us.
- CPU export `partial=false`, `exit=0`; 1528 complete PresentMon frame intervals.
  Assign samples to `[CPUStart, CPUStart+FrameTime)` after QPC-origin conversion.
  Start is absolute QPC milliseconds, not ticks or capture-relative time.
  21141 main-thread sample weights assigned;10 outside selected full intervals.
- Fastest382 frames average14.215355ms; slowest382 average18.627453ms:
  wall-time gap4.412097ms. Main leaf exposure11.791 versus15.702 nominal sampled
  ms/frame. Sample weights are statistical exposure, NOT exact execution time,
  recoverable savings, or additive inclusive costs.
- Sample module delta slow minus fast: EXE+1.2173; d3d9+1.0864; ntdll+0.7592;
  kernel+0.3325; vcruntime+0.2251; NVIDIA D3D9+0.0969; PhysXCore+0.0236;
  NxCharacter+0.0157 nominal sampled ms/frame. The `d3d9.dll` basename bucket
  includes both proxy and system symbols; do not call it entirely mod overhead.
- Capture DLL0B59770A differs from accepted installed B97B4477. These historical
  figures cannot establish current frame times or the effect of later fixes.
  Old recorder lock/EyeParams paths and native constant/binding filters have
  already changed; do not present their old exposure as a new optimization.

Input SHA256:

- cpu-raw-all.json:179929cebfafbb499c7badfc954ce90feeec76b1fb1d27b8160a44a00a7fadea
- presentmon.csv:7a39e6f3341aa287973468caf50863ad2dd2e2a4d039b4abdff004a306101af2
- qpc-origin.json:99456b5b7e459910d75354c437d6d2b6cc83ea75c24b70b8957a0c0827a15465
- private combat-owners.csv:63afb2ccedf0313340018cbb13bd83a23ca681a5ab38d0791871a08d955dca36

## Native source evidence

Read-only Ghidra queries against exact EXE SHA256
D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1.
Mapped4813 native PCs;476 have no analyzed function and stay UNKNOWN with their
individual PC retained. Do not infer owners from nearest address. Truncated
stacks and incomplete function discovery prevent ruling out all unseen work.

Six functions selected from growing exposure were disassembled/decompiled:

- 006C74A0: native geometry submission, including COM vtable+0x15C SetStreamSource,
  +0x1A0 SetIndices and +0x148 DrawIndexedPrimitive, with mutable renderer state.
  Inclusive exposure0.314->0.479 nominal ms/frame; exclusive0.0157->0.0628.
  The inclusive delta0.165 is not a pure mathematical task or an offload saving.
- 0073F570: flushes accumulated primitive state, issues DrawPrimitive through
  COM+0x144 and clears mutable batch fields. Inclusive0.243->0.395.
- 00A87BD0: selects 006C74A0 or an object callback, sharing renderer context.
  Inclusive0.141->0.356 overlaps the geometry path; do not add these figures.
- 00B4D2B0: selects shader/material variants, references shared containers and
  shader creation routines, and mutates renderer caches. Closest-native-caller
  exposure0.144->0.228. It is not an isolated pure shader transform.
- 004291B0: buffer finalization through COM+0x28/+0x30, clears active state.
  Closest-caller0.325->0.424 includes descendants, not just its short body.
- 00776DC0: broad mutable engine update/dispatch orchestration, not a single
  safely movable function. Inclusive0.233->0.387 overlaps descendants.

Native worker evidence:

- PCs0094481D/00944200/00944719 belong to00943F50, a sizeable numeric task with
  mutable output blocks. It accounts for3053 exclusive combat sample weights
  distributed across15 non-main TIDs; no main7828 leaf samples in that function.
  Do not claim a specific gameplay subsystem without further type evidence.
- 009AA571 is `CALL ECX`;009AA573 is its return in009AA4D0. It dispatches a
  task function and performs completion/free-list bookkeeping. Its descendant
  stacks total4005 weights on the same15 non-main threads.
- Earlier proof remains: BD3D10 queries processor count and creates native
  workers;906E40->AF22A0 consumes IOCP tasks. This is not a single-thread-only
  engine. Adding another thread does not remove its render-owner dependencies.

Private exports (outside Git):

- `%TEMP%/d2vr-combat-growing-functions.txt`, SHA256
  6b1c78493156e4b14206191142c00c4bdda6e7b911256e3d214ed36155df5520
- `%TEMP%/d2vr-native-worker-body.txt`, SHA256
  c4b24d342816340ddb271c9a4e84f154ebc200259cfa1fbddc58f1dad7325865
- ownership input/export/script: `%TEMP%/d2vr-outdoor-native-owners/`.
  Export script checks exact EXE SHA before querying `getFunctionContaining`.

## Tail frames and limits

Worst complete frames are38.0981/30.9117/29.1389/29.0164/28.8017ms.
Their PresentMon GPUBusy is6.2720/6.8205/6.2583/5.5553/6.7386ms respectively.
This supports investigating CPU submission/scheduling rather than lowering
resolution. PresentMon CPUBusy includes non-wait application interval and is
not thread RUN time. Several spikes recur approximately5s apart; attribution to
a particular diagnostic timer or synchronization event is NOT proven. CPU
export has no retained switch/ready arrays, so do not invent lock owners/waits.

## Reproduction and next boundary

Run `tools/perf/compare_sampled_frame_costs.py CAPTURE --owners PRIVATE_CSV
--pid 26912 --tid 7828 --output PRIVATE_OUTPUT`. Output currently retained at
`test-output/performance-audit-20261103/frame-quartiles.json` and contains input
hashes, module/leaf/native/first-caller/inclusive quartile exposure. Inclusive
columns overlap and must not be summed. Unknown owners keep separate PCs.

PASS: three parser fixtures independently check timestamp boundaries, foreign
thread exclusion, recursive stack deduplication, partial-input rejection and
overlapping-frame rejection. Real capture attribution and git diff check PASS.

Next worthwhile investigation is redundant geometry/material/buffer submission
across a whole renderer phase and its synchronization, rather than another
small matrix worker. A change needs proof of the same visual/state/HRESULT
behavior plus a current cost witness. Existing VS/PS/binding filters already
remove known redundant setters; resource-cycle shortcuts, forced visibility,
unsafe UI-copy removal and moving D3D9 calls without ownership proof remain
closed. Existing data does not yet authorize any particular runtime shortcut.

Installed DLL was rehashed during this investigation and remains
B97B44775EAC979221FBAF9476F9A6D568B01B48681935B1A1A1CCE7DE926BC2.
