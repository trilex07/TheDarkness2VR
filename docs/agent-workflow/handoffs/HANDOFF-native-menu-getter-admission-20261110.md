# Native603190 getter detour: exact admission refusal and completed CPU proof

Status: ongoing product task, NOT frozen/Steam-ready. No runtime, GPU, input,
installation, lease or REA used. MAIN owns Wonderlands; this slice is CPU-only.

## Proposed route evaluated before implementation

603190 is a27-byte native thiscall leaf, SHA256
`e9c127f7d17c85159bc87d51c7aec652cb00fec8ab9114a7072931508bb045f2`.
It reads mouse+21C/+220, writes the two caller output pointers, RET8. It has no
calls and no in-method prologue/hash check. Input getter43E160 tail-dispatches
mouse virtual+6C to603190, preserving the real Flash updater callerCB7259.
CB7249..CB7256 binds outputs to ESI(movie)+E4 and +E0, stored in EDI/EBP;
CB7259 reads those values before the existing movie transform callCB727A.
FC00C8+6C remains603190. Keeping that slot unchanged is necessary but does NOT
make an inline detour admissible.

### Exact blocking branch, not a guessed hash exception

Current diagnostic1172 has ImagePreservingPresent=1 and
BlockNativeImagePatches=1. LoadConfig latches `g_nativeImageGuard` before hook
installation to `{true,0x400000,0xDEC000}` from the PE headers. In the actual
`src/native_image_guard.h::Guard::Allows(target,length)`, the final branch is:

`return target+length <= base || target >= base+bytes;`

At target603190, length5 (and at60318B,length80 for existing MinHook external
interceptor span), BOTH terms are false. The main image is the half-open range
`[0x400000,0x11EC000)`. This rule is based on WRITE ADDRESSES, not on matched
prologue bytes, method names, a caller lease or whether the vtable stayed native.
The wrapper `NativeImageWriteAllowed` reports a refusal; existing external
interceptors likewise call `guard.Allows(p-5,80)` before MH_CreateHook.
There is no603190 installer to execute: this CPU proof evaluates its necessary
write admission before implementing it. No claim of an actual runtime refusal.

Actual CPU checks now assert denial of603190/5,603190/1B,60318B/80,43E160/5 and
CB7259/5, with external-address positive control and guard-OFF negative control.
Turning the guard OFF would admit the span; that is neither authorized nor a fix.
No transient restoration, native-validation patch or bypass was added.

Distinguish this mod policy from native dynamic address derivation:603190 itself
has no hash call, and the two previously reproduced native CRC profiles A/B do
NOT cover603190. Reusing the exact recorded-crash model, a hypothetical5-byte
JMP only in a local bytearray leaves both known profile CRCs/decoded addresses
unchanged. That is a bounded negative for TWO profiles, not a global integrity
guarantee or proof that native callers never hash other regions. It does not
override Guard::Allows. Existing7 historical CRC regressions also PASS.

## Concrete CPU source changes and results

- `test/native_flash_cursor_test.cpp`: seven additional controls against the
  actual production Guard (no mock/reimplemented predicate).
- `tools/prepare_native_flash_fixture.py`: adds exact603190,43E160,821400 and
  FULL9A5D90 method pins, mouse slot+6C, and explicit RET8/RET4 boundaries.
- `tools/native-camera/audit_input_service.py`: corrects its purported full
  9A5D90 range from178 to17A. The former ended in the first byte of RET4 at
  9A5F07; complete end is9A5F0A. New full SHA256:
  `67e02e71a4e562cd5c8ce6e55708d4e1c3f15e65495187ffe9be3724cc170515`.
  Constructor821400/79 and getter603190/1B independently end in RET8.
  Historical reports describing178 as complete are superseded on this point;
  their previous math/poll layout findings are unaffected.
- No production renderer, native code/vtable, OS pointer, ClipCursor, button,
  native transform or control profile was changed in this slice.

Run `./test/build_native_flash_cursor.ps1 -OutputDir <absolute private output>`.
`captures/native-flash-alignment/guard-getter-v7.log`: exit0, **51102 checks PASS**,
including51005 actual native SSE comparisons; old mapping RED remains255/183,
required GREEN666/509, actual event/button preservation retained. Eight native
method pins, three slots and four rounding constants PASS. Static input-service
audit with NO --pid also PASS; no process access.

## Remaining safe integration blocker

The existing borrowed GetDeviceData call at return9A5E70 would admit a private
mouse instance only while the native owner is actually on that call stack,
with ESI/owner/device/window/buffer checks. But the S21 readonly receipt does
not contain the external COM implementation target/module/bytes, and there is
no installed borrowed-mouse callback. A readonly object snapshot is not that
lease. No safe product transport is currently proved under the unchanged guard.

The existing hardware execution Boundary was source-inspected only, NOT armed
or changed: it uses a singleton debug slot/current thread and would be a
different interception route. This slice does not silently substitute it for
the requested existing borrowed-mouse lease. No new framework/profiler/hook.

Consequently the old static FDA6E8+148 prototype remains blocked and must NOT
ship, and the proposed native603190 inline detour is now explicitly closed as
inadmissible. Correct coordinate math alone is not a finished native menu fix.

## Minimal actual-camera trial pinned for MAIN, no current grant

Separate recipe (same one-behavior UnifiedPadPackets build, no Dot/menu mix):
`../.horizon-packet-20261110/docs/agent-workflow/handoffs/HANDOFF-unified-pad-camera-trial-20261110.md`.
Source6b13e042, DLL2D8397F2, exact manifest85CD2184 unchanged. Proposed future
single8-minute owned session: actual world gate, existing30-second readonly
CameraShakeTrace,6s neutral /250ms XR RY /8s neutral with constant declared HMD
pose, no recenter during measurement. Native pitch angle need not return to0;
the outcome is RATE after neutral. No physical-source collision is forced by
this XR-only scenario; symptom stability does not establish the user's cause.

Archived source-ownership evidence (550281-byte PID7288 log SHA D70520F6) is
`../.horizon-packet-20261110/captures/horizon-packet/user-log-source-ownership/`:
original historical owned copy retained, exact new archive verified,97 relevant
line-indexed records.55 XR Pad rows,8 source=XR,0 source=physical,0 remapping;
conditional edge-only source logging cannot exclude unlogged physical input.
No origin from a newly read live log is asserted.

Working reload/throw/Cinema/settings and installed81FD/761F/1172 preserved.
Reported horizon cause and real shot/render owner correspondence remain OPEN.
No complete Steam candidate or final task freeze is declared.
