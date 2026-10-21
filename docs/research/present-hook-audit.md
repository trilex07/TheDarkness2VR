# SwapChain Present hook audit (Darkness II, Oct 2026)

## Previous technique (failed)

- `PatchVTable(swapChain, 3, Hook_SwapChainPresent)` modified **slot 3 of the shared `IDirect3DSwapChain9` vtable** in `C:\WINDOWS\system32\d3d9.dll` (`sharedVt=64E112D4`, `origPresent=64E53E30`).
- This replaces the global Present slot for **all** swapchain instances and breaks the Steam overlay hook chain (`GameOverlayRenderer.dll` loaded per WER).
- Symptom: first `SwapChainPresent` sometimes logged, then `DarknessII.exe` exit ~8–10s, WER fault `steam.dll` `0xC0000005` @ `0x000341f7`.
- Heavy `BeginScene`/`EndScene` CSV tracing caused render-thread `0xC00000FD` (stack overflow) — separate failure class.

## Current technique (stable session, hook may not be on ETW present path)

- **Per-instance shadow vtable**: copy 32 vtable slots, replace only `[3]`, repoint **one** swapchain COM object’s vtable pointer to the clone (`technique=shadow-vtable`).
- **Install only after successful `Device::Reset`**, with **restore before Reset** (undo clone, `VirtualFree` clone).
- **`PresentHookLevel=0`**: pass-through — call original Present first, then atomic counters only (no COM queries, no CSV, no VrFrame on `SubmitMode=0`).
- Result: **91s stable** with `HookMask=2`, but **zero** in-process `SwapChainPresent` counter hits in the same run.
- **PresentMon 2.6** (ETW, `HookMask=0`): **21,743** D3D9 present rows, `SwapChainAddress=0xF76E9E0`, median `FrameTime` ~2.83 ms (~354 Hz composite metric — includes compositor samples; treat as ETW reference not exact game simulation Hz).

## Interpretation

- `GetSwapChain(0)` COM pointer (`~0F6CA500`) **≠** PresentMon `SwapChainAddress` — ETW address may not match COM `this` or the game presents through an internal swapchain not returned by `GetSwapChain(0)` on the hooked CreateDevice instance.
- Engine device global `@0x010DEE90` reads as invalid (`0xC9`) during early boot; rendering uses the hooked CreateDevice path for EndScene/PassProbe but Present path alignment remains unproven.
- **Non-invasive ETW (PresentMon) is the current authoritative presentation trace** until in-process hook targets the same object PresentMon observes.

## COM signature verified

`IDirect3DSwapChain9::Present(this, const RECT*, const RECT*, HWND, const RGNDATA*, DWORD)` — `STDMETHODCALLTYPE`, index **3**, `HRESULT` return, original invoked once per frame in pass-through mode.
