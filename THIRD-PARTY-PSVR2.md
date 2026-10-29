# PSVR2 adaptive triggers — third-party notices

The x86 client, x64 bridge, and Toolkit backend in `src/psvr2/` are adapted from:

- KHARVOX (`src/psvr2/`), Copyright (c) 2026 CactusVRStudios, MIT License.
  https://github.com/CactusVRStudios/KHARVOX
- SingularityVR adaptive-trigger client shape (`psvr2_client.h`), Copyright (c) 2026 letsgosportsteam, MIT License.
  https://github.com/trilex07/SingularityVR

Runtime integration calls the PSVR2 Toolkit C API (`psvr2_toolkit_init`,
`psvr2_toolkit_deinit`, `psvr2_toolkit_get_driver_active`,
`psvr2_toolkit_set_trigger_effect`, and
`psvr2_toolkit_loader_get_module_handle`), Copyright (c) 2026 Bnuuy Solutions, MIT License.
https://github.com/BnuuySolutions/PSVR2Toolkit

`psvr2_toolkit_capi_loader.dll` is not vendored. Place the Toolkit build next to
`Darkness2Psvr2Bridge.exe` at runtime. `src/psvr2/TriggerEffectAbi.h` is an
ABI-compatible command struct for that C API and is not a copy of Sony's
pad header.

MIT terms: include the copyright notice and permission notice with substantial
copies; the software is provided "AS IS", without warranty of any kind.
