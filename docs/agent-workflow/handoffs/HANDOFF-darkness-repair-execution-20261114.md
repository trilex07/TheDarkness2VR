# Handoff: Darkness II VR repair execution — startup evidence

**Date:** 2026-11-14
**Role:** SIDE, offline as requested
**Branch:** codex/darkness-repair-plan-20261114
**Plan:** [Darkness II VR repair plan](PLAN-darkness-repairs-20261114.md)
**Public runtime source:** `331d5b41` (installer 1.1.1, catalog 1.1.8)

## Phase 0 — baseline verified offline

- Verified the downloaded 1.1.8 catalog with the installer tool and repository publisher key, and matched the downloaded VR core ZIP SHA-256 to its signed entry: `DC5E3E6E3DC337F866D5F6D0559EC508E24F4D69B1586E760582030418BF9EF6`. Independently hashed each ZIP stream.
- The public runtime and the current local Steam installation are different. Current Steam files were read for identity only. Do not install the public bundle over the local E6B8 baseline just to satisfy a comparison; use a staged trial with explicit MAIN authorization and fresh rollback.
- Raw public INI is the pre-transform package payload. The installer derives `PolishText` and `AppliedPolishText` from the selected game text module (see installer `src/Trilex.Installer.Core/DarknessLanguageSettings.cs`); an installed INI hash may differ. Preserve the user's profile. The unmodified control settings in the public archive are `[Render] FrameBoundaryPolicy=0`, `StartupMode=2`, `DisableOpenXR=0`, `SubmitMode=4`, `StereoMode=1`, `[Diagnostics] ImagePreservingPresent=1` and `[Simulator] Enabled=0`. Ignore the identically named keys under `[Input]` for Render diagnosis.

| File | Installer 1.1.1 archive | Local Steam snapshot 2026-11-14 (prefixes) |
| --- | --- | --- |
| `d3d9.dll` | `6F913407289093107388109F624B7BCAF1030189D46988F4B16334E0021C5D27` | `E6B8B605` |
| `Darkness2VR.ini` | `22862511B4E5697F37413802D3B562DD388ED510C68D0E1CD745E629A5A79B08` | `49FA1AFD` |
| `Darkness2VR-diagnostic.ini` | `117230AC717C2A5FE7C1CA82BF6A693B2A056855D4B47F18CCA941325462C076` | Same |
| `openxr_loader.dll` | `D4DD992E067EAABBDFA651C90EB4578E44DB6651FB4539AEC0964DBC22BCFDBA` | Same |
| `DarknessII.exe` | Game-owned | `D46BB6CB` |

E6B8 is the prior user-tested B3F/unified pad build from `2b101b98`. The active local profile has `[Input] NativeFlashRayCursor=1` and `UnifiedPadPackets=1`. Historical observation: main-menu laser present, pause laser invisible, and gameplay laser below shots. The local log was last written on **2026-11-10**, with native Present readiness, OpenXR session running and repeated layer cadence. It has no `XRStartup` lines. It is neither a fresh 1.1.1 run nor the affected player's report. No mix of these three evidence sets is valid.

As of this read-only snapshot `vrserver` was running and Darkness II was closed. The **x86** HKLM ActiveRuntime registry view selected `virtualdesktop-openxr-32.json`; the x64 view selected `steamxr_win64.json`; both JSON files existed. There was no `XR_RUNTIME_JSON` in the current shell; that does not establish the game process environment. SteamVR running or its x64 manifest being registered does not establish the runtime used by this x86 game. The runtime name logged by a fresh run is the useful witness.

## Phase 1 — pending new runtime evidence

The old external report proves the signed 1.1.7 install transaction and native Present qualification. It has no positive OpenXR session/frame witness. The reported executable fingerprint differs from local MAIN. Neither startup mode, runtime vendor nor executable identity has been shown to cause the failure. No startup behavior changed here.

1. **Local MAIN, if separately authorized later:** record headset, runtime route and current file hashes. With the game closed, stage a fresh exact backup of the current EXE/profile identities and every file the trial would change, including managed assets if running an installer transaction. Keep a save snapshot and rollback map of original files/absences. Verify the backup before testing. For a clean public reproduction, use the verified 1.1.1 DLL and installer-equivalent profile; a DLL-only test with the local INIs is a separate profile. Keep E6B8 and its historical saves independent of this trial. Open the headset on its intended route, start the game once through Steam, note whether the actual 3D title follows any language step, and confirm both eyes through Continue. Stop on regression; restore the exact freshly backed-up state when the game is closed. No unattended input is needed.
2. **Affected player:** install the 1.1.1 Darkness update with the game closed and keep the existing headset/runtime setup. Run once through Steam. Record headset/runtime route, whether the 3D title appeared past the language selector, whether it was visible in the headset, and whether Continue reached the world. After a normal exit, extract only the relevant startup markers below from the fresh file. Share only those lines after reviewing for path or identifying data. Do not ask for unrelated logs or change runtime settings to obtain a passing run.
3. **Acceptance:** record the last confirmed stage and first absent or failing stage. Require separate evidence of a scene in both eyes on local MAIN and on the affected player configuration to close P0. Successful `xrEndFrame` alone does not establish headset display.

PowerShell evidence commands for the person conducting the trial (read only, use the real install directory):

```powershell
$game = 'C:\path\to\Darkness II'
'DarknessII.exe','d3d9.dll','Darkness2VR.ini','Darkness2VR-diagnostic.ini','openxr_loader.dll' |
    ForEach-Object {
        $file = Join-Path $game $_
        if (Test-Path -LiteralPath $file) {
            '{0} {1}' -f $_, (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash
        }
    }
$log = Join-Path $env:LOCALAPPDATA 'Darkness2VR\d3d9.log'
Get-Item -LiteralPath $log | Select-Object Name,LastWriteTime
Select-String -LiteralPath $log -Pattern '\[Startup\]|\[EffectiveConfig\]|\[XRStartup\]|xrCreateInstance failed|\[XRSession\]|OpenXR session ready|XR session running|xrEndFrame failed' |
    ForEach-Object Line | Select-Object -Last 70
```

| Last evidence | Next check |
| --- | --- |
| No new log or no new loader/startup header | Verify timestamp, installed DLL hash, executable actually launched and whether the language window is a launcher. |
| Native Present forwarded and observer ready, but no `readyPresent` | `ImagePresentBefore` logs only on an eligible ready callback after `GetDevice` succeeds. Inspect nearby ownership/reset notices and whether an eligible Present followed; no proof of XR attempt. |
| `readyPresent`, no `initAttempt` | Inspect the logged request/reason and effective policy, then protected-presentation and frame guards in `ApplyBoundaryDecision`/`VrFrame`. Do not disable safeguards. |
| `initAttempt` without `instance` | Check the adjacent loader/instance failure or protected observer rejection. Keep the x86 registry view distinct from runtime name. |
| `instance` through session but no frame | Read the first failing OpenXR result, graphics requirements/DXGI errors, session events and wait/begin stage. |
| `first xrEndFrame` without `first game layer submitted` | Inspect successful layer production and XR visibility/pose. Empty submission does not constitute stereo game content. |
| `first game layer submitted` | Ask for actual per-eye menu/world visibility before marking acceptance. |

Sources: production `src/image_present_runtime.inl` (`ImagePresentBefore`), `src/d3d9.cpp` (`InitXR`, `VrFrame`, `ApplyBoundaryDecision` and end-frame path), [public audit handoff](HANDOFF-darkness-public-audit-20261114.md), and [E6B8 follow-up at 0dbfbdbe](https://github.com/trilex07/TheDarkness2VR/blob/0dbfbdbe/docs/agent-workflow/handoffs/HANDOFF-headset-followups-20261110.md). No new Steam launch, source/installed binary changes or headset test took place in phase 0/1.

## Offline deliverables and next physical check

- P1 Main menu: separate source branch `codex/darkness-main-menu-20261114`
  based on the exact public runtime. The configuration is off by default;
  enable only for a planned Main-menu check. The patch does not add cursor
  ownership to Options or Pause. See that branch's handoff for its DLL identity
  and fixture details. Neither the patch nor diagnostics shipped to 1.1.1.
- P0 support: installer `2a4ec38` on `codex/darkness-local-diagnostics-20261114`
  adds an explicit local, curated startup report (109 Core tests, 8 WPF layouts).
  It still needs a fresh report from an affected player, and is not itself an XR fix.
- Historical phase 3A visual appender: 49,455 assertions rerun from
  `0dbfbdbe` with appender/test files identical to `75e245ca`. This does not
  verify the current public line or a Pause click. Phase 3B still needs a
  same-frame join of the visible native pause movie and that instance's input.
- Historical phase 4 shot witness: 698 checks rerun from `bb90f327`. Existing
  diagnostic tests are sound at this scope; the real shot/render join, horizon
  and ladder remain open. No gameplay or performance patch applied.

Once the headset is available, do one manual baseline check followed by one
off/on Main candidate check against the suitable exact profile and rollback.
Verify OPTIONS highlight in both eyes, one R2 action, ordinary pad/mouse,
miss/hold and focus-loss/held-trigger refusal, a fresh press after release, and
Continue into the world. Leave Pause unrouted until independently proven.
