# Material binding windows

Opt-in `Render/ReplayConstantBindWindow=1`, requiring both existing constant
grouping options. The submission package toggles it with L3+R3. Defaults OFF.

Each original upload is transformed for the replay eye before entering owned
VS/PS banks. Only SetTexture, SetTextureStageState and SetSamplerState may
intervene between these uploads. These setters do not consume float registers;
their order and arguments remain unchanged. No pointer or resource content is
cached. All draws, shader changes, render targets, viewports, state blocks,
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
