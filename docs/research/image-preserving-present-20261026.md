# Image-preserving Present/stereo candidate

Base `a69f404`; branch `codex/image-preserving-present`. IMPLEMENTED and OFFLINE
VERIFIED. Steam/actual overlay and PSVR2: NOT RUN. No merge or installation.
This replaces the architectural dependency responsible for the two reproduced
CRC-derived-pointer crashes; it is not a live stability or image-quality claim.

## Actual correction

`src/image_present_observer.h` installs x86 MinHook trampolines at implementation
entries in **external image modules**, never in the main EXE. The native D3D9
object, its vptr, its complete table and its private interfaces remain native.
No public-length table copy, wrapper object, inferred table extent or slot-3
replacement is used by this observer. Existing replay/device hooks still change
their individual public slots outside the EXE, subject to the same image guard;
they do not clone tables. Refused required graphics hooks close XR admission.

Acquisition follows the already verified engine route:

- `00964C40`: additional swapchain acquisition; the public Device slot13 call
  returns at `00964CC1`. Only that pinned caller admits native acquisition.
- Successful QueryInterface for SwapChain9/9Ex observes the returned real
  interface's public QI/Present implementation entries. Unknown/private IIDs
  retain their exact original results and ownership.
- `00A94020`: engine presentation selection; native dispatch returns at
  `00A94057` or `00A94074`. Only these callers may open a game-frame boundary.
  `src/image_present_runtime.inl` validates the admitted file SHA/PE, load base,
  original `A94052`, `A9406F` and `964CBF` instruction bytes, read-only.

Each distinct implementation owns a distinct typed callback and immutable
relocated trampoline, published before hook enablement. Forwarding never looks
up an original function in a live vtable. Existing inline JMPs can be relocated,
including an overlay forwarding to another observed implementation. A repeated
target/object in the current Present path latches failure instead of forwarding
the recursive cycle again. Arguments and the original return value otherwise
pass unchanged. Nested distinct implementations do not duplicate frame work.

Module pins and hooks last for the process lifetime. No raw swapchain registry,
retained swapchain references, Release replacement or live hook removal is used.
GetDevice's temporary reference lasts only through that Present. Release,
address reuse, QI identity and private methods remain the native implementation's
responsibility. Reset closes readiness; successful Reset needs another actual
successful Present. Failure, a thread change without Reset, unsupported entry
memory or incompatible method ABI closes XR admission. This is synchronous
device-thread interception, not asynchronous D3D9 work.

MinHook writes external DLL instructions in process memory; this is still a
detour, not a claim of a patch-free process. Disk files are not patched. Entry
admission includes the possible patchAbove span. Executable private allocations
are deliberately refused. Unsupported overlay entry shapes fail closed; actual
Steam overlay compatibility remains a live gate. External software that changes
the game image itself is outside this mod's write guard.

## Protected stereo admission

`ImagePreservingPresent` is diagnostic-only and OFF when absent. Requesting it
**always latches BlockNativeImagePatches**, even if the latter INI key is zero.
The original image-write prohibition is retained, including EXE imports/data and
class tables. No native CRC/decoder state or hash inputs are rewritten.

The profile requires SubmitMode4/Stereo1/OpenXR ON/PresentPrimary and rejects
unsafe native hands, aim, gameplay, FOV, sizing and EXE Present opt-ins. Initial
controller bridges and native head movement are disabled. Runtime XR additionally
requires typed acquisition, a successful real Present warm-up and execution
inside the admitted presentation callback. EndScene and device fallbacks cannot
open this gate. Warm-up recording is discarded; the next complete game-frame
recording supplies the existing live/replay eye pair. Capture is OFF initially;
if subsequently authorized, its completion still requires the matching original
Present return to be D3D_OK. A recursive fault rejects pending capture.

`profiles/image-preserving-vr/` contains explicit opt-ins and a minimal override
fragment. It is not a replacement for the complete working INI. The private
package merges it over the preserved installed INI and verifies every other
setting unchanged. No resolution, FOV calibration, sharpening or shader algorithm
was changed. Native sizing remains disabled: requested eye size is not proof of
actual engine RT0 resolution. Hands/targeting cannot be declared recovered until
their image-preserving replacements have separate evidence and acceptance.

## Offline evidence and concrete artifact

- `build-image-present-observer.ps1`: real x86 fixture DLL, no GPU/game/runtime.
  Tests the actual observer/MinHook code, not a second forwarding model. Original
  tables are snapshotted before installation. Fixture-owned private slots156/37,
  distinct base/Ex interfaces, private three-slot IID, 1000 Release/recreate cycles,
  address reuse, failed/successful Reset and retained Ex references PASS. Eight
  thousand ordinary native Presents plus one intentional recursion leaf run;
  5000 frame callbacks/3000 accepted success results. Native leaf runs exactly
  once per ordinary call, despite a pre-existing JMP/overlay/Ex chain.
- Separate processes prove EXE entry refusal before patching, unexpected HRESULT
  preservation/fault closure, and thread-change closure. Test EXE text remains
  unchanged; whole-image boundary/overflow policy tests also PASS.
- Existing Present ABI/rollback/witness PASS; tracked gate1329 PASS; frame suite
  2327551 assertions/zero failures; capture31, readiness8, transfer3, camera7,
  swapchain6 and exact crash-pointer7 tests PASS.
- Both production and diagnostic x86 DLL builds PASS. Persistent logging and
  `src/stereo_replay.inl` are unchanged; shader safety branch0f9c5f5 stays separate.

Private candidate: `dist/image-preserving-present-20261026/protected/` contains
the diagnostic DLL and merged INIs; `manifest.json` records file SHA-256, exact
overrides and unchanged installation hashes. Diagnostic DLL SHA-256:
`ee0f3ca28ef29993ddda786b03e6a01a58973c88ae024f5488360095b628da24`.
Production regression DLL (not the protected candidate):
`0d6713ba17836915bf0d77c46a3ad257624d495d4a8e8e0f49224ada177aa283`.

EXE retains admitted SHA `d46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1`.
Installed DLL/main INI/diagnostic INI/local INI match the existing
`perf-log-ab-20261026-105015` rollback manifest. No game assets, EXE/dumps, private
logs or compiled artifacts are committed.

Next gate: separately authorized Steam verification of native acquisition,
successful forwarding, overlay and Reset on this exact protected package, then
same-frame stereo/capture and physical PSVR2 acceptance. Preserve the baseline;
on failure exit the candidate process and restore backed-up DLL/INIs with hashes.
Do not enable native gameplay hooks to get past a rendering failure. No Steam
launch, deployment, live success or HMD acceptance is authorized by this receipt.
