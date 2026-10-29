# OpenVR declarations

Unmodified Valve header and BSD-3-Clause license, retrieved 2026-10-29:

- https://raw.githubusercontent.com/ValveSoftware/openvr/master/headers/openvr.h
- https://raw.githubusercontent.com/ValveSoftware/openvr/master/LICENSE

SHA-256:

- `openvr.h`: `1e6ed57199896cc1f7c5484e50fa18955e97be15be690beb28d998c877ead7fd`
- `LICENSE`: `f56ff606104d4ef18e617921a75c73ad73b5a1a1d70c69590c29de16919e04ad`

The refresh reader uses declared ABI types and IVRSystem_026. It dynamically
loads the user's installed x86 SteamVR API; no OpenVR binary is bundled.
Binary packages must include LICENSE as Darkness2VR-OpenVR-LICENSE.txt.
`build.ps1 -Install` copies that notice; manual deployment must do the same.
