# SteamVR physical refresh fallback

`Display` previously accepted only XR_FB_display_refresh_rate. A runtime
without that extension could not supply the physical Hz needed by the
72/90/120 policy. predictedDisplayPeriod remains deliberately excluded: it
may already reflect throttled application pacing.

The successful OpenXR query still takes precedence. Otherwise the session
host checks xrGetInstanceProperties.runtimeName for SteamVR, then starts an
owned OpenVR background client. Valve documents that application type as not
starting SteamVR or keeping it running. The reader obtains the connected
HMD's Prop_DisplayFrequency_Float via the official IVRSystem declaration.
It neither submits frames nor calls IVRSettings or compositor control APIs.

The absolute DLL path comes from HKCU Software/Valve/Steam SteamPath plus the
conventional SteamVR/bin/win32 installation. Missing/custom installation,
already-loaded OpenVR client, unavailable IVRSystem_026, disconnected HMD,
property errors and nonfinite/invalid Hz produce UNKNOWN. There is no guessed
vtable index, version fallback, implicit 90 Hz, scan or game-local DLL lookup.
An existing OpenVR client is not shut down or adopted. Only a successfully
initialized owned background client gets a matching ShutdownInternal call.

Frequency is reread every two seconds on the existing session thread, not
every draw. Disconnect/error clears the previous value; reconnection can
restore it. Stop/loss invalidates it and closes the owned client outside
loader lock. Successful xrEndSession also ends the adaptive controller's old
session state. OFF/AUTO/HALF semantics and XR frame sequencing are unchanged.
There is still no supported application-scoped SteamVR half-rate adapter;
the UI continues to report UNSUPPORTED, without fake frame limiting.

Evidence on this machine: the compiled x86 optional read-only probe returned
90.000 Hz from the already-running runtime, then stopped its background client.
No game was running or launched, no window/scene was created, and no settings
API was used. This proves the native property-query path; the integrated
OpenXR session fallback still requires the consolidated game/headset session.

Offline: refresh/lifecycle tests 409 checks PASS; adaptive policy 3,243 PASS;
VR Settings pointer/input 4,518, actual renderer 863, actual input host 50,
and actual submission 7,215 PASS. Both complete x86 builds PASS.
The adaptive integration fixture compiled, but Windows refused execution with
sharing violation; the EXE later disappeared. No quarantine/exclusion was
changed. This execution is NOT RUN, not an assertion failure or a PASS.

Primary reference and redistribution terms:
https://github.com/ValveSoftware/openvr/blob/master/headers/openvr.h
and third_party/openvr/LICENSE. No upstream implementation code was copied.
