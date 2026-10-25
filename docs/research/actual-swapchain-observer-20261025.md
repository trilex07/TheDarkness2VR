# Actual swapchain acquisition: verified path, rejected observer

Branch `codex/actual-swapchain-observer`, based on `beb60b7`.
Result: **Steam observer gate FAIL; matched capture NOT RUN**. The failed
prototype was removed from runtime source and the installed baseline restored.
No EXE patch, shared-vtable patch added by this experiment, FOV change, or merge.
Existing baseline proxy hooks were not redesigned.

## Acquisition and lifetime evidence

Private Ghidra analysis of the historical B56B sample located these functions.
Their instructions were then independently disassembled from the currently
installed D46B original using Capstone. Five hashed instruction ranges also
match the prior audited 55C8 original byte for byte. These are build-specific
addresses, not portable SDK symbols.

- `00964C40`: prepares D3DPRESENT_PARAMETERS; at `00964C98` uses owner+1C as
  output. `00964CBC` loads device vtable+34h; `00964CBF` calls **slot 13,
  CreateAdditionalSwapChain**. Return address `00964CC1`.
- `00964CF4`: output owner+20; `00964CF8`: IID at `0107C2F4`;
  `00964D00`: calls the base swapchain's **QueryInterface**. IID bytes decode
  to `91886caf-1c3d-4d2e-a0ab-3e4c7d8d3303`, IDirect3DSwapChain9Ex in the
  installed Windows SDK. This is not GetSwapChain(0).
- `00964D17`: GetBackBuffer slot 5, retained at owner+24.
- `00B6A670`: initializes owner fields; calls acquisition at `00B6A6AB`.
- `008B63C0`: releases backbuffer+24, base+1C and Ex+20 through slot 2,
  clears the fields, and reacquires through `00964C40` at `008B6416`.
- `00A94020`: only presents when owner+28 is set. Uses Ex+20 preferentially,
  base+1C otherwise. Slot 3 calls at `00A94055` / `00A94072`, returns
  `00A94057` / `00A94074`; the dirty flag is cleared afterward.
- `00920EE0`: presentation selection includes alternate Device Present/Ex
  paths; its swapchain branch enumerates handles at +104..108 and calls
  `00A94020` at `00920F65`.

This proves a native acquisition path and its explicit release/recreate
sequence. It does **not** establish this run's actual additional-swapchain
pointer, its Present owner, or whether an overlay intercepts that Present.

## Controlled test and exact failure

Fresh installed EXE audit passed all 14 sites, 18 previous ranges and the five
new ranges; Authenticode status Valid, Valve signature. SHA-256:
`d46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1`.
No other game process was running; SteamVR/fpsVR remained untouched.

One Steam launch, 2026-10-25 08:20:58 local, PID 19856. Diagnostic candidate
SHA-256 `EED491A02C5B61ADB8065AC64858EAC2FEE025FCB396E75A9872DF44CED2DDCE`.
SubmitMode=4, StereoMode=0, DisableOpenXR=1, opt-in observer on; capture off.

The rejected prototype cloned each public interface table, saved original
functions separately, used stdcall forwarding, observed QI/Release, detached
on Reset, and refused distinct public interface aliases. It did not retain COM
references. Its fake-COM tests passed, but missed the native implementation's
private virtual methods. Public COM interface length was an invalid assumption.

Runtime observations before failure:

- Device `0F6BF640`, original vtable `0F6C259C`, shadow `0F6A6F18`.
- Original device QI `6479A0F0`, Release `64785AF0`, CreateAdditionalSwapChain
  `647FCE30`; all resolved to system D3D9. This identifies the acquisition
  implementation, not the actual swapchain Present implementation.
- Implicit swapchain `0F625E60`, slot 3 `64763E30`; actual additional chain
  remains **UNKNOWN**. No acquisition callback was reached.
- Access violation C0000005 at EIP=0; EAX=shadow table, EDI=device, ESI=0;
  first return on stack `647688BD` = system D3D9 +488BD.
- Independently disassembling the hash-verified x86 system D3D9 shows
  +488AA `mov eax,[edi]`, +488AD `mov esi,[eax+270h]`, then +488BB `call esi`.
  **270h / 4 = private slot 156, outside the 134-entry public Device9Ex table.**
  The short shadow supplied no valid function there. This explains the null
  call; it is not evidence that Steam overlay caused it.
- Present entries/forward/return observations: **0 / 0 / 0**, no HRESULT
  witness. Equality at zero is not a passing gate. No COMPLETE.json.

The module file is `%WINDIR%/SysWOW64/d3d9.dll` (32-bit process logs its
redirected system32 path), PE I386, preferred base 10000000, image 179000,
timestamp 3F41597F. SHA-256:
`fd72e021c128a98a5d688eb9acc07e3eb1948ca6b630b9fa5f328898e78c7dbc`.
ASLR addresses above belong only to this run.

The run stopped on this failure; no second candidate or stereo run. Private
backup/evidence directory: `%TEMP%/d2vr-scobserver-20261025-082057`.
Rollback verified installed hashes:

- DLL: `9C96DC85F8F0B32C97411D71871A0E2FBBFA27C2818569F986AE78551A72334C`
- Game INI: `C802439A7AD84E6B4CEA567405B3A5B85BEEA89EB0A00DBF7313FA5437E77436`
- Diagnostic INI: `586F536AFEF7BA596F6F2D8789A74B6B37F20501B4902FD3EAF4D33C5E1DA9C0`

## Committed validation and next blocked gate

`tools/native-camera/audit_swapchain_dispatch.py` reproduces the EXE identity,
five acquisition/lifetime ranges, module identity and exact private dispatch
bytes. It does not load/execute either binary. Unknown files/bytes fail closed;
a known private slot is never promoted to a complete table extent or safe hook.

```powershell
python tools/native-camera/audit_swapchain_dispatch.py '<game>/DarknessII.exe' --d3d9 C:/Windows/SysWOW64/d3d9.dll
python -m unittest discover -s tools/native-camera -p 'test_*.py' -v
python -m unittest discover -s tools -p test_matched_capture.py -v
```

Offline: 13 PE/dispatch tests (7 existing + 6 new), 15 capture tests PASS;
frame simulator 2,327,551 assertions / zero failures. Both x86 DLL variants
build again after removal. `src/`, build.ps1 and standard INI are byte-for-byte
unchanged relative to beb60b7. Existing untracked files preserved.

**Blocker:** no safe verified observer of the actual additional swapchain.
The native device has private virtual dispatch beyond the SDK interface.
Blindly increasing the clone to 157 or scanning until a non-code pointer is
not a verified fix; total extent, private ABI and wrapper chaining are unknown.
The swapchain implementation could have analogous private methods. Do not
deploy another public-length shadow or revive the old recursive slot-3 hook.
Further work must establish a non-truncating interception strategy and actual
object/overlay ownership read-only before another candidate. Capture/checker/
Luna on real frames and PSVR2 acceptance remain NOT RUN. Installation/AV audit
passes; that does not clear the observer safety gate.
