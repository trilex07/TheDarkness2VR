# Approved mono trial — 2026-10-25

Tested source: `7b5ee3d99d404c68db80e1fb213dd4adfd306ce2` on
`codex/diagnostic-engine-present`. Explicit approval covered mono only,
OpenXR/capture disabled, rollback required. No VR follow-up was run.

## Result and limits

**Actual Present boundary: PASS in intro/title screen.** Last sampled counters:
44,000 entries = 44,000 forwarded = 44,000 returned; recursive=0, fault=0.
47 logged return samples, all D3D_OK. No logged exceptions or FAIL lines.
Full HRESULT history is **NOT PROVEN**: this exact candidate logs the first
three and each thousandth return. Do not claim all 44,000 returned D3D_OK.
Before a further candidate, log/count every non-D3D_OK result so an intermittent
error cannot escape the stop condition between samples.

The game showed moving intro and title-screen imagery. Loaded gameplay was
not tested. Enter did not reliably advance from the title screen; no claim of
menu/gameplay input acceptance. Alt-F4 did not terminate the process, so the
known test PID was stopped at the title screen after verifying its image path.
Shutdown through the game's menu therefore remains unverified. This bounded
trial proves forwarding, not a complete gameplay/VR regression pass.

No matched frame, COMPLETE.json, ray comparison, or new PSVR2 observation.
Next VR launch remains unapproved.

## Identity and runtime evidence

- Fresh EXE SHA: `d46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1`;
  PE32/I386, timestamp 4F68A875, base 00400000, image DEC000, original call-site
  bytes and all existing 14 sites / 18 ranges passed. Authenticode Valid.
- Exact approved DLL SHA:
  `89FD7DFC4967894783F136D23AF687BDBA8D0F6BD0B06FA4FAE8655CE5B4BFE0`;
  deployment hash matched its backed-up candidate.
- PID 11036, start 09:36:36 local; render thread 26060.
- Effective SubmitMode=4, StereoMode=0, DisableOpenXR=1, StartupMode=2,
  HookMask=0xF, PresentHookLevel=1; MatchedFrameCapture=0, CaptureSerial=0.
  No native FOV change; other native hooks remained compiled out.
- Installed in-memory call-site A94052, rollbackUncertain=0; exact caller
  A94057. Device Reset succeeded, epoch=1, 1280x855.
- Actual swapchain `0F8A0FE0`, vtable `647212D4`, owning device `0F806C00`.
  Implicit swapchain `0F781760` is different.
- Present target `64763E30`, module logged as system32/d3d9.dll (x86 path).
  Slot target ownership does not prove absence of an inline overlay trampoline.
- Present witness logged current SwapChainPresent entries and successful
  corresponding forwarded returns. No EndScene fallback was substituted.

## Rollback receipt

Private evidence: `%TEMP%/d2vr-engine-mono-20261025-093634/` contains inventory,
original copies, approved candidate, audit, mono.log, summary.json and RESTORED.
No raw logs, game assets or binaries committed.

Restored and verified:

- DLL `9C96DC85F8F0B32C97411D71871A0E2FBBFA27C2818569F986AE78551A72334C`
- Game INI `C802439A7AD84E6B4CEA567405B3A5B85BEEA89EB0A00DBF7313FA5437E77436`
- Diagnostic INI `586F536AFEF7BA596F6F2D8789A74B6B37F20501B4902FD3EAF4D33C5E1DA9C0`
- Local INI unchanged `2320B450FAEF34A17DE4A7E0A3E55D2A14A8DE98772E13B6F8A497EAA5FDDA20`
- EXE SHA unchanged after process exit. No disk EXE modification, reinstall,
  antivirus change, native FOV change or merge.

## Prepared next capture, not executed

Track the user's prior PSVR2 feedback separately: `docs/issues/VR-001-right-tentacles.md`,
`VR-002-yellow-periphery.md`, `VR-003-detail-loss.md`. These are not observations
from this mono candidate, and exact prior build/config provenance is unknown.

One future approved loaded-gameplay frame should contain both right tentacles
and a reported yellow edge with a textured distant surface. Keep the source
camera/FOV settings fixed; do not sweep tuning parameters.

1. Close the HRESULT observability gap in the isolated candidate offline;
   run existing recursion/capture tests before proposing its exact DLL hash.
2. After separate approval and fresh backup/audit, enable the existing matched
   capture with SubmitMode=4, StereoMode=1 and one new positive request serial.
   XR remains forbidden in this completed mono trial; do not apply that config now.
3. Require source-eye0/1 and final-eye0/1 from one complete gameplay frame,
   matching witness/device/thread/reset epoch, draw banks, viewports, crop,
   projections, XrFovf and poses. Check COMPLETE.json and all hashes first.
4. Compare right-tentacle visibility first across the four images and their
   draw records. If absent already in source, do not blame D3D11; if absent only
   after replay/blit, constrain the investigation to that transition. Presence
   in the draw stream alone is not proof of visible pixel coverage.
5. Locate the yellow region at the same stages. Only then test a specific
   culling/lighting/postprocessing hypothesis with a narrow follow-up capture.
6. Run Luna's ray-aligned comparison only with independently supported source
   camera/FOV and pose correspondence. If the checker rejects that evidence,
   record the rejection and obtain the missing witness, not guessed tangents.

This prepares the next experiment; it neither authorizes it nor claims fixes.
