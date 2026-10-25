# Post-restart Steam / Present receipt - 2026-10-25

Branch: `codex/runtime-camera-readiness`, following `cbe3661`. This receipt
supersedes the previous inability to start the game, not the open capture gate.
No renderer, FOV, native hook, runtime selection or production INI source change.

## Controlled attempt

The user confirmed the reboot and requested continuation. Before deployment,
the installed original 55c8 SHA passed PE identity, all 14 sites and 18 instruction
ranges. Steam RunningAppID was zero; no game or VR process was running. Existing
DLL, game INI, diagnostic INI, local INI and log were copied and hash-verified.

The committed diagnostic DLL built successfully (x86), deployed SHA:
`0e133a81eefede80bbc0e3bbc2c0d1ae019f95a18e4794cead84933779b9cfc4`.
Probe configuration: SubmitMode=4, StereoMode=0, DisableOpenXR=1, StartupMode=2,
HookMask=15, FrameBoundaryPolicy=0, PresentReadiness=1, MatchedFrameCapture=0.
Game INI probe SHA:
`9fffeed4c111b7e1f2e5c22d9a8d50b7892f008fe69fe2fbb80b52daf4da4510`.
This is a mono COM-coverage probe, not a stereo or matched-capture test.

Steam exposed a usable window after its initial launch helper timed out. A single
`-applaunch 67370` request then started DarknessII PID 21244. Steam's own content
log reports completion of an already queued update: 327 updated files, BuildID
11598, successful commit immediately before game startup. MAIN did not request
another integrity verification, restore a quarantined EXE or change AV settings.

The game reached its title screen and main menu, observed through Computer Use.
After detecting the new EXE hash, further testing stopped. The title/menu process
did not exit through the attempted close/quit controls; only this test PID was
terminated, before loading a save. No gameplay, OpenXR or PSVR2 validation.

## Real COM coverage observation: not capture-ready

The DLL loaded from the intended directory and logged its effective probe config.
CreateDeviceEx and a successful Reset were observed (1280x855, format 22). All
33 replay hooks reported installation. Native gameplay/FOV hooks and the EXE
Present dispatch patch were suppressed by the diagnostic build.

Last sampled counters: **39,000 EndScene calls, zero DevicePresent, zero
PresentEx and zero SwapChainPresent callbacks**. Device slots 17/121 still
reported the installed hook addresses after Reset, throughout the sampled log.
The implicit swapchain was returned successfully, but its slot 3 was not hooked.
There were no Present witness entry/return records. These are title/menu counts,
not 39,000 proven game frames and not a gameplay visibility test.

This confirms the existing observer's coverage gap during this run. It does not
identify the actual dispatch object. Adding the old engine-callsite wrapper to a
COM slot would still recurse, and EndScene is not an acceptable substitute.
Next runtime gate: a safely forwarded, lifetime-managed observer of the actual
COM presentation path, with real call/return evidence before requesting capture.
No COMPLETE.json was requested or produced, and Luna gameplay analysis was not run.

## Identity change investigated offline

Post-launch installed EXE SHA:
`d46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1`.
The previous strict single-hash audit correctly rejected this file initially.
Both samples are 14,291,512 bytes, PE32/I386, base 00400000, timestamp 4F68A875,
image size 00DEC000, entry RVA 00A1CDAB, with identical section layouts.

Static comparison against the privately preserved 55c8 sample found:

- Entire .text identical: 11,442,176 bytes, SHA
  `e7eed44cbfa7424de59b9b79a7206404ed3d6d1da7a6710f3332d12d7ddb56d4`.
- All 14 sites and 18 instruction ranges match independently.
- .data, .tls, .version and .reloc are identical.
- 161,799 file bytes differ overall, including 161,205 bytes in .rdata; headers,
  .rsrc and certificate data also differ. The cause/meaning of these non-code
  changes is not inferred from their location.
- Windows Authenticode verification reports **Valid** for BOTH files, signer
  `Valve Corp.`, certificate thumbprint
  `451458C2AD1210744969CED42D98AB98B91E345E`.

The audit now explicitly recognizes these TWO full-file SHA values and emits an
identity label. It still rejects arbitrary hashes even when every selected site
and range matches. This is recognition of the observed, signed Steam variant;
it does not prove camera ownership or authorize native FOV changes. No executable,
certificate blob, shader dump, raw private log or trace is committed.

## Rollback and validation

After the test process exited, the previous deployment was restored and verified:

- DLL: `9c96dc85f8f0b32c97411d71871a0e2fbbfa27c2818569f986ae78551a72334c`.
- Game INI: `c802439a7ad84e6b4cea567405b3a5b85beea89eb0a00dbf7313fa5437e77436`.
- Diagnostic INI: `586f536afef7ba596f6f2d8789a74b6b37f20501b4902fd3eaf4d33c5e1da9c0`.

Steam's newly installed EXE was preserved. The original local INI was not edited.
Private backup, runtime log, initial failed audit and RESTORED.txt are under
`%LOCALAPPDATA%/Temp/d2vr-readiness-20261025-065020`.

Audit regression suite: 7/7 PASS, including matching instruction evidence with
an unlisted full-file hash. Read-only audit of both actual samples: PASS. A
non-code byte mutation of the new sample is rejected despite matching sites and
ranges. The diagnostic build passed; renderer code has not changed since cbe3661.

Steam startup: PASS. Existing COM Present coverage: FAIL in the title/menu run.
Matched capture / ray-aligned gameplay comparison / PSVR2: NOT RUN. A fresh
identity audit is still required before any later launch; queued Steam updates
can replace a file after an external preflight. No further launch in this attempt.
