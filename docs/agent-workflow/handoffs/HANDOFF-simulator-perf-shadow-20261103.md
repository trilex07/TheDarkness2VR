# Handoff: Simulator performance and shadow investigation

**Date**: 2026-11-03
**Task ID**: simulator-perf-shadow-20261103
**Role**: MAIN
**Agent**: Codex
**Branch**: codex/simulator-perf-shadow-20261103
**Worktree**: C:/Users/Trilex/Darkness2VR/.main-camera-shake
**Baseline**: accepted Combined + IsolatedEyeOutput, source 81bddf2b; installed E56BF027… / INI 68EE5685…
**Integration commit**: pending; NOT accepted for main

## Goal / status

User authorized cloning elliotttate/OpenXR-Simulator and unattended real-game
testing to fix FPS and head-relative shadows/reflections, without lowering
resolution. IN PROGRESS: simulator works; one shader candidate implemented and
tested for admission/algebra/driver creation. Neither visual shadow correction
nor a sustained FPS improvement is proven. No performance fix claimed.

## Context loaded

- BASELINE.md, FAILED-APPROACHES.md, WORKFLOW.md; relevant artifact and lighting handoffs.
- OpenXR-Simulator README/source and local UI automation API.
- Host game automation, performance, VR mod and reverse-engineering guidance.

## Changes / provenance

- f6cb30a3 restores src/INI to accepted 81bddf2b and adds process-only simulator
  selection, default OFF in diagnostic INI. No global OpenXR registry switch.
- Clone `tools-local/OpenXR-Simulator` at 692a66de570189b29c684f48c6ef306a07d5764f;
  official Vulkan-Headers dependency. Built x86 with VS18 2026; ctest22/22 PASS.
- New `shadow_clip_coordinates.h`, startup `[Diagnostics]ShadowClipCoordinates=1`:
  OFF by default; admitted only with geometric head + SubmitMode4/StereoMode1,
  excludes older ShadowScreenDepth replacement. Creation failure uses original.
- C4D1FB10 bytecode:568 DWORDs, SHA256
  7fb0cc4d119b3f93336344f4d44bb3907d79465371fa3ef5d67c4062734bb751.
  Word234 `[03000005 80030000 80e40000 80000001]` multiplies NDCxy by
  sampled DepthBuffer.x before InverseProjection c27..30. Candidate changes
  word234 to02000001 and237 to0: MOV xy + NOP. Full original FNV/size/version/
  instruction/END gate; all unrelated tokens preserved. Native .95 projection
  conversion at AD4021..AD4067 intentionally retained, not guessed away.
- New local shader archive/disassembly, simulator controller and algebra tools;
  x86 thread-context sampler. `simulator-lab.ps1 Update` retains original backups.
  Game-derived bytes/images and cloned repos ignored, never committed.

## Evidence (private, relative to worktree)

All lab outputs below: `test-output/simulator-lab-20261103/`.

- First lab PID50820 DLL274…: matched captures901/frame32722 and902/frame35153.
  Native camera VP identical; head yaw .7->.9, pitch-.4, fixed position. Files
  validated; effective existing inverse matrices delivered (error<9e-7).
  This proves delivery, not lighting input ownership or image correctness.
- Candidate PID44116 DLL SHA256
  3DD141F666D30CBE093355E9C575FCC7AAE581427C948E4F5A6528D2ACE48A65:
  protected diagnostic build PASS; driver accepts replacement, log result00000000.
  Matched903/frame74995 validates with effective C4 constants. Captures904/77238
  and905/110971 validate files but C4 absent from the consuming draws; they are
  NOT a successful two-view image acceptance pair. 905 saved other pass shaders.
- `audit_shadow_clip.py`: three points, both eyes, three suitable captures901,
  902,903. Old residual .030–.184 game units; candidate .00159–.00166 against
  geometric reference retaining native bias. Algebra PASS only. Assumes sampled
  depth is homogeneous clip z; prove actual producer/encoding next.
- `build-shadow-clip.ps1 -PrivateShader <lab>/shaders/ps-C4D1FB10.bin` PASS:
  exact mutation, unknown/truncated rejection and actual D3D9 driver creation.
  Candidate bytecode SHA256 9AC17314040BC9DC3D6D8C8A825A838D088784A53BEC19CCEA0AA3980B30EB21.
- Standard build PASS:3F2E5D965F37634B4C3D9F06369637A77D4C9F28368A841EA3C0B366B46D7EB4.
  DO NOT install standard into protected profile. Capture validator34/34 PASS.
- Cube reflection pair7DCA4CC4/12CCC8DB: captured World matrix and
  ModelSpaceEye_Time.xyz remain identical under pure head rotation. No evidence
  to justify blanket edits to model-space eye position or world transforms.

## Performance findings and limits

- `fps-clean-villa.json`: first lab stationary villa89.95fps over10.006s,
  p50 frame9.199ms/p9912.936ms. Actual source2676x2728, XR3232x3300 per eye,
  same as accepted native/swapchain configuration; no resolution reduction.
- Candidate stationary `fps-stationary-no-capture.json`:85.26fps, p5011.564ms,
  p9916.376ms over10.005s, different view/location. NOT controlled A/B gain.
  No40fps combat reproduction or physical SteamVR comparison achieved.
- `fps-with-capture901.json` includes891ms screenshot hitch. Misleadingly named
  `fps-clean-candidate.json` is also POLLUTED by UI screenshot/movement/cinema
  transition (59.11fps); do not use as stationary baseline or regression proof.
  Simulator GPU timing is null; its `app_busy` label is not ETW CPU execution.
- Latest no-capture boundary mean~3.3ms, producer fence~.08ms, reader~.01ms;
  timestamp GPU spans are NOT GPU Busy. Logs contain AsyncLog drops166, so
  missing entries cannot prove no stall or support a complete critical path.
- Concurrent Polish TTS Python PID38304 used~3 cores at measurement time;
  untouched. Started18:26 local, AFTER original reported issue. Not its proven
  cause. SteamVR processes also left running untouched.
- WPR CPU.Verbose+GPU.Light FAILED policy privilege0xc5585011, even outside
  filesystem sandbox. No valid ETL collected. Directory perf/20261103T173751Z-3808922e.
- Standalone command-history arm/export worked:1,324,879 writes, retained32768,
  missingEnds0, foreignMutations0, trigger0 (no recorded call>=12ms). Retained
  ring is only the tail of the interval; no whole-session conclusion.
- `thread-context.json`:156 wall-clock IP samples of render TID51268 over5s;
  all previous suspend counts0, every suspend balanced by resume. Separate
  modules-x86.json fixes 64-bit host module inventory limitations.69 samples
  game,49 ntdll,14 system D3D9; remaining runtime/driver/etc. These INCLUDE
  blocked/descheduled time; not a CPU execution profile or causality proof.
  Snapshot API wall overhead max14.05ms invalidates FPS claims in that interval.
  Native observed IPs BB7B2D, AC3800/09, C5D8D0 are next callgraph leads ONLY.
- Villa false cinema reproduced: frame80233 native0/video1 while free walking.
  Existing known BinkWait lifetime failure; no speculative timeout/dimension fix.

## Final installed state / recovery

Owned test PID44116 closed (WM_CLOSE failed, exact PID/start-time/path checked
before termination). `tools/simulator-lab.ps1 -Action Restore` PASS. Steam:

- DLL E56BF027875A6FF3997E0B6A51A0DC43F52DD86C2EB39AE79D4C5EAB073FA5CA
- INI 68EE5685C7AE9FEC8F1AE2677D551DD90C7BCC21DD66737560815EEF4AB26EA9
- Diagnostic INI AC8290E48D6CD8CA34ED935734E7293572E2F9227B0AA27D4CA03507FA619299
- EXE and openxr_loader protected hashes match lab receipt. OpenXR ActiveRuntime
  still SteamVR steamxr_win64.json; simulator opt-in removed by exact rollback.
- CONTINUE.SAV, EE.cfg, Editor.cfg unchanged. Only user settings changed through
  native execution; saved settings-after-lab, restored pre-lab settings hash.
- Diagnostic recorder OFF. No user game launch pending. Candidate remains local
  lab payload, OFF by default; no main merge or SteamVR acceptance.
- Lab receipt/payload and rollback originals retained. Shader caches updated
  normally during testing; NOT a fully isolated cache A/B. Do not claim otherwise.

## Next steps

1. Prove C4 DepthBuffer source/encoding and save its effective input, then obtain
   a controlled same-body two-head pair where that shader consumes pixels in
   both captures. Current candidate has no visual acceptance and covers one PS.
2. Attribute a reproduced sustained slow frame with CPU running/ready/wait data;
   native render/callgraph leads above are not permission to multithread live
   engine owners or remove synchronization. Need elevated WPR token or another
   validated profiler route; preserve resolution, stereo, hands/tentacles/FOV.
3. Resolve world Bink fullscreen presentation ownership independently. Do not
   exclude world-camera frames wholesale or infer fullscreen from dimensions.
