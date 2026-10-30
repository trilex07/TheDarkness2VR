# Right-eye replay: StateBlock construction is not live state

Baseline: MAIN integration `69b02db1de04001cc49f31f25c4593923cfd1993`,
merged into `codex/permanent-right-eye-flicker`. Retains MAIN recorder
synchronization and the earlier accepted-resource bind correction.

## Source defect and correction

The device hooked EndStateBlock but did not hook BeginStateBlock. Consequently
state setters used only to construct a StateBlock were recorded as live changes.
For example, the live eye retained texture A while constructing a block containing
texture B; right-eye replay then bound B without any corresponding live bind.
Shader metadata, constant histories and stereo constant rewriting also treated
construction as live execution. This is a concrete source inconsistency affecting
UI-style state setup, independently of buffer contents or projection mathematics.

[D3D9 BeginStateBlock](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-beginstateblock)
records listed setters into an object rather than applying them to live state.
Track successful Begin/End calls for the exact device. During construction,
forward all hooked capturable setters unchanged, exactly once, with their native
HRESULT. Do not append replay commands, inspect captured textures as live depth,
publish shader metadata, transform constants or invalidate live histories.
Keep recording state on failed Begin/End calls, and clear it on successful End
or device Reset/ResetEx. Render-target/depth binds, clears and draws retain their
existing handling because they are not StateBlock-recordable setters.

The optional Apply hook and its layout heuristic remain unchanged. This correction
does not establish complete coverage of opaque Apply calls when that hook is off.
No INI/profile, geometry visibility, WVP/STP, FOV or native camera changes.

## Before/after regression

`test/build_live_resource_binding.ps1 -SourceRef 69b02db1` extracts the actual
baseline texture/depth hooks. The fixture models the documented native capture
semantics and executes actual replay switch cases. It fails check 1656, line 147:
`left.textures[slot]==right.textures[slot]`. This is an expected baseline failure.

With corrected source, the same fixture passes 1,679 checks. Its six construction
cases cover texture slots 0, 7 and 256 with binding filtering both enabled and
disabled, no phantom records, unchanged depth observation and balanced references.
`build_stateblock_recording.ps1` extracts complete production Begin/End hooks
and verifies hook installation: 12,004 checks, 1,000 build cycles, failed/nested
Begin, failed End, exact device identity and native return/call receipts.

Additional PASS results on x86:

- Live setter status: 5,880 checks, including 16 capturable setters with raw
  arguments, four HRESULTs, no replay records or recorder locks during construction.
- Pixel shader metadata: captured shader binds preserve live metadata and records;
  existing creation/reuse/concurrency tests pass.
- Native constant source: 9,368 checks including captured PS constants preserving
  carry state and native forwarding; existing ABI and producer tests pass.
- Live StateBlock shader synchronization: 296,171 checks.
- Projection history/WVP/STP/FOV: 145,248 checks; shader register bank: 783,041.
- UP geometry: 4,403; resource reference lifetime: 10,000 frames per mode;
  replay state receipts: 184; replay surface receipts: 128 streams.
- Right-eye color history, material constant window, protected hands (2,233),
  left aim (461), tentacle head scope (26,629).
- `build.ps1` and `build.ps1 -DiagnosticCapture`: PASS, no installation.

Build artifacts in the isolated worktree (SHA256):

- `d3d9-right-eye-stateblock-production-x86.dll`:
  `A216700132F489BC220EC7B3AFFA8B16E3066EAF97EE50DD73603F5564D55611`
- `d3d9-right-eye-stateblock-diagnostic-x86.dll`:
  `ED8BD50106780845B6338DE61B3D2A4025B3954D41EA13ADB9989D59C7567415`

## Performance and verification boundary

No added GPU operations, GPU synchronization, driver queries, allocations or
setter mutexes. Ordinary setters add an atomic pointer load, comparison and
branch. Build-phase setters avoid existing recording/metadata work. Runtime CPU
cost and FPS have not been measured; synthetic timings are not performance proof.

The user reports red dots, brief unrelated elements, rectangular blood/HUD and
intermittently disappearing right-eye menus. Current available captures do not
establish that BeginStateBlock executes at those exact glitches. The offline
before/after proves this source defect, not elimination of all observed artifacts.
PSVR2 menu/HUD/blood/stereo acceptance remains NOT RUN and belongs to MAIN.
No game launch, Steam deployment or headset verification was performed.
