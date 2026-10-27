# Reader-fence follow-up

Apply this single Render override to the full protected-cpu-call-reduction INI.
The user reported previously severe drops around 30 FPS improved toward 50 FPS
with the three-option baseline. This is user feedback, not a controlled timing
measurement or attribution to one option. Preserve that baseline for rollback.

LastConsumerReaderFence=1 issues the existing D3D11 event after the shared
texture's final read and unbind, rather than after unrelated later work at the
next overwrite. The D3D9 overwrite still requires successful completion of that
same dependency. Producer fencing, both eyes and rendering commands remain.
The existing helper retains pending dependencies on timeout/error and validates
context, query and thread ownership; reset releases the old resource lifetime.

Fresh x86 helper regression and all five transfer-order tests pass. The current
DLL already contains this implementation: no rebuild or new code is required.
Runtime benefit remains unmeasured. The latest user's baseline log shows reader
wait means around 0.2-0.26 ms per read, two reads per pair; that is exposure, not
an achievable saving or a promise of 90 Hz. GPU timestamp spans are not GPU Busy.

Apply off.overrides.ini and restart to return to the three-option baseline.
Keep the original full DLL/INI backup separately. Compare the same outdoor route
and verify stereo, tracking and absence of flicker before keeping this option.
