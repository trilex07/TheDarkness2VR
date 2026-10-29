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

The following MIT permission applies to the copyrights listed above:

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
