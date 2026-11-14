# Handoff: public Darkness II language and startup audit

**Date**: 2026-11-14  
**Role**: SIDE / offline  
**Branch**: codex/darkness-public-audit-20261114  
**Baseline**: 7aa2359f, public 1.1.7 installer catalog pinned runtime A14CA636. The accepted Steam baseline is separate.

## Outcome

The public DLL used fixed Polish VR Settings labels even with PolishText=0.
Ported the language-dependent labels to the exact public source, including its
original Controls page. Missing INI flags now default to English. The installer
already derives PolishText from the explicit Polish text module and uses distinct
EN/PL native menu recipes. The translated panel follows that choice. The public
bundle does not ship the private language profiles, so in-game archive switching
is not part of this candidate.

Reviewed the external flat-start report. The supplied log in the audit establishes
native Present readiness, but does not establish an OpenXR attempt. On the public
source, readiness opens admission for the next eligible native Present. Its before
callback gets the device, feeds SwapChainPresent to the boundary machine and calls
VrFrame after the existing guard and boundary checks. StartupMode=2 skips native
gameplay hooks and FOV probing, but does not directly gate InitXR. No executable
fingerprint, instruction guard, startup policy or frame scheduling was relaxed.

Added bounded XRStartup receipts for the boundary decision, initialization,
runtime identity, system/session, initial wait/begin/end results, and first
game-layer submission. Existing startup, observer and session-state receipts
remain in place. A layer submission is not headset display acceptance.

## Offline verification

- MSVC x86 /O2 DiagnosticCapture build SHA256
  6F913407289093107388109F624B7BCAF1030189D46988F4B16334E0021C5D27.
- Settings input, real EN/PL GDI labels, host routing, submission and language
  helper fixtures passed. Native Present x86 ABI/admission, recreation, Reset,
  HRESULT and ownership tests passed.
- Companion installer catalog 1.1.8 and complete local installer candidate 1.1.1;
  107 installer tests, eight offscreen WPF layouts and embedded PSVR2 client smoke passed.
  All other archives, runtime INIs and loader are byte-identical to 1.1.7.
- No Steam install, game launch or headset test; no raw external logs committed.

## Next evidence and stop conditions

1. P0 remains open. On an affected player, establish whether the actual 3D game
   menu appeared; after exit inspect d3d9.log for XRStartup. If no readyPresent,
   examine observer ownership and reset; if readyPresent but no initAttempt,
   examine the logged boundary reason and existing guards. If an OpenXR call
   failed, inspect that exact stage/result and runtime, rather than changing EXE
   admission or switching runtime unconditionally.
2. Before P1/P2 optimization, capture a current hash-pinned scene and align CPU
   RUN/wait, GPU Busy, replay and XR cadence. Reuse existing shader-demand,
   packet preparation and transfer ownership paths. Only change a path for
   which the current sample supplies a bottleneck or an exact eye defect.
   No support for repeating AFR, broad shader constants, global AF, DLAA or
   frame generation on the evidence in this audit.
3. MAIN and affected-player headset validation remain separate. A dedicated
   sanitized log ZIP is future work; request only the focused startup lines.
