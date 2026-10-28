# CPU geometry replay correction

The original UP geometry hooks can record different inputs from the live draw:
indexed capture copies NumVertices from the base, ignoring MinVertexIndex;
point lists copy PrimitiveCount+1 vertices; arithmetic can wrap; oversized/null
fallbacks retain caller pointers until replay. D3D9 requires UP data only until
the call returns, not until our later second-eye pass. See Microsoft's
[indexed UP contract](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-drawindexedprimitiveup)
and [primitive types](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dprimitivetype).

The new hooks snapshot the exact input range under one recorder lock, calculate
counts without unsigned wrap, and preserve original indices/MinVertexIndex.
The unused indexed vertex prefix is allocated and explicitly zeroed locally;
only the actual requested vertex range is read. This works even when the
caller's unused prefix is PAGE_NOACCESS. Replay uses arena offsets exclusively.
Unrepresentable payloads stop recording with the existing overflow flag rather
than preserving an expired pointer; replay completion also retains that flag.
Original live draw calls and HRESULTs are preserved, with no extra driver calls.
The existing 8 MiB per-payload limit remains; huge sparse index ranges can exceed
that limit and are not represented as a complete stereo pair.

Before correction, actual hooks/replay extracted from 26a97cf fail check 11:
`rendered==expected && driverCalls==2`, with a nonzero minimum vertex index.
After correction, actual-source x86 tests pass both index formats, nonzero
minima, every primitive type, overwritten source memory, unreadable unused
prefix, initialized arena padding, arithmetic refusal and original-once calls.
This fixture models vertex fetches; it does not exercise a graphics driver.
Final verification: 4344 UP checks PASS, existing recorder regression 6096
checks PASS, replay surface status 96 streams/576 calls PASS. Both production
and DiagnosticCapture x86 DLL builds PASS.

The retained recent game counter had indexed UP=0. Thus this fixes a definite
replay defect but is NOT identified as the user's observed flicker root cause
or an outdoor performance bottleneck. Ordinary dynamic VB/IB content versions,
partial/inherited constant banks and physical HMD acceptance remain open.
No new setting or deployment is performed. MAIN should include it in the next
combined native-resolution test with the preceding controller and shader fixes.
