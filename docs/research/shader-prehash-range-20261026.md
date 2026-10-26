# Pixel shader prehash range correction

Base: `8359d0149780d80c68fd260383f433973573c572`.
Status: IMPLEMENTED / OFFLINE VERIFIED. Steam and PSVR2: NOT RUN.

The eager hash in that candidate reads every byte before native shader creation.
`SafeShaderTokenCount` reads instruction boundaries and can skip a complete
NOACCESS or GUARD page inside a comment, then find a readable END. Its nonzero
count therefore did not establish that hashing the complete range was safe.
The exact prior hook now reproduces C0000005 in the private regression fixture.

The PS creation hook uses a bounded validated token walk. It queries each
encountered virtual-memory region afresh, including regions skipped by token
payload lengths, before reading the next token or admitting the full range for
hashing and bytecode patches. It rejects guard, no-access, uncommitted,
non-readable, overflowing and inconclusive ranges. No permission state survives
the call. The unchanged VS token walker is outside this correction.

Uncertain input has UNKNOWN source identity: no diagnostic/composite/FXAA patch
is attempted, and the original device, bytecode and output arguments still reach
native CreatePixelShader. Its HRESULT is preserved. If native creation succeeds
despite our uncertainty, a recycled pointer cannot retain its old hash; existing
metadata extraction may use the driver's own GetFunction output. No shader
AddRef/Release, asynchronous creation, native cache ownership or hash-only reuse
was added. Exact-target ShadowScreenDepth still rereads its diagnostic INI on
each creation; unrelated materials retain the earlier INI/scratch avoidance.

The source remains caller-owned and must stay stable throughout the synchronous
D3D9 call. Fresh permission checks do not solve concurrent VirtualProtect,
deallocation or mutation by another thread. SEH is a last-resort token-read
failure guard, not the mechanism that preserves a pre-existing PAGE_GUARD.

## Focused verification

`test/build_shader_create_hook.ps1` compiles actual production helpers and hook,
plus the pinned `98442ef` baseline, against the existing deterministic native
API fixture. Both pass. Added cases cover:

- A readable header/comment and END separated by a NOACCESS or GUARD page.
- GUARD on the initial-token or END page, without consuming the guard bit.
- Fresh validation after the same allocation changes protection.
- Exact original arguments, one native forwarding, failure HRESULT and untouched
  output pointer, no invented source hash or shader metadata.
- Opaque native success preserves HRESULT/reference ownership and uses only
  native GetFunction output; stale source hash identity is removed.

Existing valid ordinary/target shader, live diagnostic INI, replacement rejection
fallback, device recreation and eye cases still pass. Preserved private shaders
33DE98C1 and 75F88E11 also pass for candidate and baseline, retaining their exact
DOF-only patch output. No private shader bytes are included in this commit.
A private control built
from the exact `8359d01` hook fails with `-1073741819` (C0000005) on the new
malformed-gap regression. These fixtures are not HAL or headset validation.

Both production and diagnostic DLL builds pass. No game files, configuration,
FOV, hands, resolution or renderer mode changed. No measured FPS improvement is
claimed. Rollback is the parent revision; MAIN owns any later integration/test.

Build receipt (SHA-256, local outputs excluded from Git):

- Production: `6F17907A531A6911B2B679EE1C1C38E3D37D353E00E7C2D379533A456A3542C6`.
- Diagnostic: `2872923FFE0A77F3996BA99FBE03814F4756F9483292797AE74A5D2EBB6E7049`.
