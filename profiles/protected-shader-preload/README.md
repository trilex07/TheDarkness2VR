# Owned startup shader preload

Merge the ON keys into the effective game INI; retain all protected stereo,
input and existing CPU settings. Restart required. OFF bypasses the pool and
learning entirely. This is separate from the earlier create-and-release warmup.

The first run seeds from `%LOCALAPPDATA%/Darkness2VR/startup-shaders.bin` if the
learned `shader-preload.bin` is absent/invalid. Successful native creations add
their actual GetFunction bytecode to the learned archive. The effective code is
captured AFTER shader replacements, not selected by a 32-bit name/hash. Private
shader bytes are not in Git and are never uploaded.

The next run creates the archived shaders once on the valid device's creation
thread, before the native engine receives it. One owned COM reference satisfies
at most one byte-exact, type-exact, same-device CreateShader request. The entry
is then emptied, without AddRef/Release of the caller's returned reference.
Repeated requests, changed code, another device, null/unsupported input and
cache failures retain the ordinary CreateShader path and its HRESULT. No native
owner slots, executable bytes, driver or OS code are patched.

VS/PS shader objects survive Reset/ResetEx, unlike default-pool surfaces:
https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-reset
The pool is suspended under its admission mutex before reset and resumes only
after success. This preserves hits across this game's immediate startup Reset.
No shader creation/binding happens inside reset callbacks. Unused objects expire
one per frame after two minutes; consumed objects belong solely to the game.
The bounded pool permits 1024 unique SM3 entries, 32 KiB each, 4 MiB aggregate
archive. Startup has a 10-second BETWEEN-CALL budget and stops creating after a
driver rejection. A running driver call cannot be interrupted by that budget.

Learning copies validated bytecode in memory at successful creation. At most one
background writer persists a bounded snapshot every two seconds when changed,
then atomically replaces the archive. It never calls D3D9, accesses game pointers
or writes files on the render thread. Startup loads validate the entire archive.
An interrupted write cannot replace the last complete archive. Workers pin the
module and use process-lifetime state; there is no loader-lock thread join/COM
cleanup. A last-second process exit can lose newly learned entries, not corrupt
the previous complete archive.

Test sequence: run the problem scene for at least 30 seconds, close the game,
restart and promptly load that same scene. Compare the second visit with a
matched baseline. The first run has only partial seeded coverage. Check
`[ShaderPreload] startup ready=...` and `requests=... hits=... learned=...
saved=... writeFailures=...` in the log. Hits prove avoided CreateShader calls,
not FPS gain or elimination of driver first-draw specialization.

Offline evidence: x86 real D3D9 REF and HAL/Ex (flags 0x54) fixtures check one-use ownership, native miss
and HRESULT behavior, effective bytecode equality, background persistence and
second-start hits, initial/successful/failed reset admission, guard-page
preservation. Core checks include 16-thread competition and a 1024-entry bound.
Run `test/build_shader_preload_pool.ps1` (add `-Hardware` for HAL/Ex and ResetEx).
The private 27-shader seed also passed three driver creation passes with zero
failures; this does not establish cold-cache or gameplay performance.
Production/diagnostic builds and prior
shader-package/stereo-ordering regressions are checked separately. No 90 Hz or
headset performance acceptance is implied by these checks.

MAIN: integrate the shader-preload commit only, preserving the existing protected
profile; retain its default OFF setting until controlled acceptance. Seed/cache
files remain private on the test machine. Roll back the deployment backup or set
StartupShaderPool=0 and restart; learned files may remain inert.
