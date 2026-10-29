# Material binding windows

Opt-in `Render/ReplayConstantBindWindow=1`, requiring both existing constant
grouping options. The submission package toggles it with L3+R3. Defaults OFF.

Each original upload is transformed for the replay eye before entering owned
VS/PS banks. SetTexture, SetTextureStageState, SetSamplerState and (since the
2026-10-29 extension below) SetVertexShader/SetPixelShader may intervene between
these uploads. These setters do not consume global float registers;
their order and arguments remain unchanged. No pointer or resource content is
cached. All draws, render targets, state blocks,
resource operations and unknown commands remain barriers. SetRenderState stays
a barrier because vendor FourCC values can initiate operations.

Look-ahead visits each intervening binding once, uses no plan allocation, and
flushes at the last original float command inside its SEH boundary. Existing
invalid-upload forwarding, failure disable, abort/no-retry and capture bypass
remain. A binding fault aborts pending grouping and disables further deferral;
faulted streams are not claimed image-equivalent. Fresh replay resets all banks.

Offline PASS: extracted actual replay loop, production and diagnostic x86, each
96 streams / 576096 records / 70090 checks, transformed bits and hard barriers,
caps, invalid inputs, HRESULT failures, SEH no retry and capture bypass. Actual
system HAL/Ex x86 flags 0x54: 400 material sequences, identical VS/PS bits and
bindings, synthetic constant calls 1600 to 400. XInput and actual D3D11 status
shader regressions PASS. Both DLL configurations build.

The diagnostic ReplayUploads receipt adds bindWindowStreams. This is CPU
submission reduction, not GPU geometry migration. No current-game FPS saving
or resolution of the user's short ~38 FPS hitch is established by these tests.
The larger renderer/90 Hz goal remains open.

## Shader binding extension, 2026-10-29

Keep every shader bind in its original order, with its original arguments and
success/failure metadata transaction. Only defer already transformed, owned
float writes across it until the next observing/mutating barrier. A later
upload still uses the shader selected at that upload, not the shader selected
when the combined packet is eventually sent. Cross-shader raw WVP references
use the shared register correction in 1e35723.

The D3D9 contract separates global API constants from shader-local `def`
constants; shader-local definitions have no external side effects. See
[Microsoft's assembly shader reference](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx9-graphics-reference-asm).
This extension changes neither shader bytecode nor creation/compilation.
It adds no cross-frame cache, driver query, allocation or synchronization.
The existing ReplayConstantBindWindow switch controls it; no INI change.

Actual-source x86 replay tests PASS in production and diagnostic flavors:
96 generated streams / 576,096 records / 70,207 checks each, plus explicit
binding-dependent VS/PS transform sequences. Those sequences reduce eight
constant setters to four with identical banks and shader bindings at each
draw. All eight shader binds still occur in order, including null binds and
each possible failed bind; failed streams remain incomplete. Adjacent-only
and window-only modes retain eight setters. Capture bypass, hard barriers,
invalid uploads, driver HRESULT failures and SEH no-retry remain covered.
The source-extraction fixture was updated to the current shared-register and
shader-binding implementation rather than testing an obsolete replay copy.

The last retained runtime window (endPair 6360, source 2676x2728) had 184,235
VS/PS constant API entries over 120 replay streams, about 1,535 per stream,
after existing batching. It does not record the adjacency needed to predict
this extension's additional savings. The 8-to-4 result is a constructed CPU
regression, not a real-game frame-time or graphics-driver measurement. No
1 ms saving, flicker resolution or stable 90 FPS is claimed.

## Independent setter extension, 2026-10-29

The same opt-in grouping now spans SetStreamSource, SetStreamSourceFreq,
SetIndices, SetVertexDeclaration, SetViewport, SetScissorRect and the four
VS/PS integer/boolean constant setters. These operations do not observe or
modify float constant banks. All ten setters still execute in original order
with original arguments; float transforms execute at the original upload,
including viewport-sensitive shadow classification. Integer and boolean banks
are separate; no source pointers survive an upload. No setting or new cache,
allocation, lock, device query or worker thread is added.

Draws, RT/depth changes, resource-content operations, FVF/transform changes,
state blocks, render states (including vendor FourCC actions) and unknown
operations remain barriers. Existing HRESULT handling is unchanged for the
ten admitted setters; this change does not make failed state setters sticky.
Shader/float failures and SEH retain their existing incomplete/abort behavior.

Actual-source CPU x86 regressions pass in production and diagnostic flavors:
96 generated streams, 576096 records and 56809 checks each. A dedicated
two-draw sequence executes each new setter twice, covers null resources and
scissor, valid viewport payloads, exact setter arguments/order, all 20 possible
setter failures, and VS transforms dependent on viewport state. OFF,
adjacent-only and float-window-only modes make 20 float calls; extended mode
makes four, with identical float banks and independent state at both draws.
This is a constructed regression, not measured graphics-driver or game savings.
Both x86 DLL builds pass. Existing runtime has not been rerun, so no additional
ms/frame saving, 90 FPS result or side-visibility/flicker fix is claimed.

## Validated public render states, 2026-10-29

The next extension admits an explicit state-and-value whitelist for ordinary
depth, stencil, blending, color masks and related boolean render states. The
[public D3D9 render-state contract](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3drenderstatetype)
defines their setup values separately from float shader registers. Float writes
may now span these setters under ReplayConstantBindWindow, while each render-state
setter keeps its original position and arguments. Transforming a float packet
still happens at its original upload. No state/value cache, allocation, extra
device query, record pass or worker is introduced.

POINTSIZE, adaptive tessellation (including vendor RESZ/ATOC-style commands),
unknown state IDs, invalid enum/mask values, state blocks, draw and resource
operations remain barriers. Both the immediate barrier check and look-ahead use
the full recorded state/value, so a later command-like state cannot be crossed.
The existing last-float flush keeps driver exceptions at the float command;
each native render-state setter still executes once after an upload exception.
The existing handling of render-state HRESULTs is preserved.

The retained installed ddd2e9d log's endPair7800 window has 181356 vertex and
149267 pixel constant API entries over 120 streams: about 2755 per replay image.
It does not retain state/float adjacency, so it cannot predict this extension's
game savings. Actual-loop x86 regressions pass 58286 checks per DLL flavor,
including the existing 96 streams / 576096 records. A constructed 16-float,
16-render-state sequence goes from 16 float API calls to two, with exact constant
bits, state and setter order at the draw, including all 16 setter-failure
positions. Sixteen command-like/invalid cases retain all 16 float calls. The
SEH case faults at the final float command, makes one driver entry, rejects the
pair and forwards all 16 render-state setters once. Adjacent packet regression
passes 80190 checks / 360000 input operations. Both x86 DLL builds pass.

This is an offline batching correction awaiting consolidated runtime testing.
No measured driver-time reduction, 1 ms saving, right-eye artifact closure or
stable 90 FPS is claimed. Installed 2e9b887 is retained; the combined next package
also contains 7f60631's located grip velocity.
