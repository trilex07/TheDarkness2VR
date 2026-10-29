# Enemy grab crash: scoped stock-path fallback (2026-10-25)

During the one approved PSVR2 trial of `e118a26`, the user reported that the
game crashed while lifting an enemy. The game EXE was the pinned x86 image,
SHA-256 `d46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1`
(PE timestamp `0x4F68A875`). The installed DLL and INIs were restored with
matching baseline hashes after the process exited. No repeat launch is implied.

The last first-chance exception was `C0000005` reading `DD91AD64` at
`00E129EA`, inside the EXE's `rep movsd`. Disassembly shows the caller
`004E0E40` copies a buffer in 64-byte blocks through `00E12990`; the exception
does **not** prove that the native throw velocity hook caused the invalid
source. The log shows a grab, a second LB throw pulse, the native grab state
becoming empty, then the exception. Throw token 4 was armed for an entity but
there was no `NativeThrow APPLIED` for that token. The prior RC12 report also
named offset `+0xA129EA` under a different FOV experiment, so the offset alone
is not a unique root-cause signature.

The native throw adapter already rejects the alternate held-character/weapon
branches when the fire object has a target in `+0x584` or `+0x590`, or the held
entity has the `+0x24` flag. The current candidate carries the same guarded
classification into the native grab sample. While an alternate hold is active,
the mod leaves the native camera ray and throw direction alone; ordinary prop
throws retain controller steering. A failed or stale branch read also retains
the stock ray. This is a narrow protective A/B candidate, **not a verified
crash fix**. It does not change the LB gesture or native enemy lifecycle.

Offline: the diagnostic x86 DLL builds and the pinned-EXE gameplay/camera site
test passes 1307 checks. Steam and PSVR2 validation of this candidate: NOT RUN.
Next physical test, if authorized, should first verify enemy lift/release with
this fallback and record the alternate-branch transition; stop on a repeat
exception. Hands remain visually frozen in the completed trial and are a
separate defect. No hand-palette changes are included in this candidate.

## Protected gesture release correction, 2026-10-29

The protected gesture path unnecessarily used `!owner.alternateHeldTarget` as
an admission condition for the entire left-hand input recognizer. Entering
the held-enemy/weapon branch therefore erased the grip/stroke and cleared LB
before a deliberate release gesture could issue the stock action. This was
separate from the intentional ordinary-prop aim/velocity ownership gates.

Actual-source regression first failed at `Step(260,0,.34f,0)` (line145):
grab pulse, enemy acquisition, powered stroke and grip release produced no LB.
The recognizer now retains the same owned camera/inventory/fire context for
this native branch. The stock LB pulse reaches the engine; its native branch
decides the action, physics and lifecycle. Ordinary-prop ray/velocity overrides
still reject alternate targets both at publication and at native consumption.
No native target, animation, velocity, code or static table is rewritten by
this input correction. The existing protected private release adapter remains
unchanged. Physical face buttons and the right-hand inputs remain available.

Validation: 218 actual-source protected throw checks PASS, including acquisition
transition, one deliberate release pulse, no prop steering token, delayed Busy
acknowledgement without repeated throws, stationary grip release without a
throw, owner changes, invalid context, preserved right stick/RB and A/B/X/Y.
Protected aim 461 checks PASS (including fresh alternate-branch refusal).
Production and diagnostic x86 builds PASS. These are input/adapter regressions;
they do not run the native held-enemy lifecycle or prove a crash fix. Physical
enemy attacks, executions, release/throw animations and trajectories remain
to be accepted together in MAIN's consolidated headset test. Controller-based
velocity steering for the alternate enemy/weapon physics branches remains
unimplemented; this change removes the blocked native action, not that limit.

## Protected character release direction, 2026-10-29

The retained Ghidra release implementation and a new read-only75AD20 export
identify a supported character velocity boundary. At8799AF, ESI receives the
native +584 reference; it is retained before879C88 and released afterward.
The comparison's velocity at stack+60 is passed unchanged to75AD20 at879CA6,
with the native mode2.75AD20 forwards that same pointer/mode to each nonempty
body through backend VT+C4. Its complete92-byte body ends at75AD7B (RET8).
The +590 retained-target branch has other side effects and remains excluded.

The protected adapter now forwards the saved native ESI reference as a third
resolver argument while retaining the existing comparison's ABI. Publication
and consumption both refresh the camera/inventory/fire reciprocal owner graph,
the exact +580 entity and +584 character. Consumption additionally requires the
native retained reference to name that character. A changed/cleared target,
retained +590 target, missing native reference, stale/expired token, tracking
loss or wrong owner leaves the original velocity intact. Ordinary prop rules
and the legacy EXE-patching path do not gain character permission.

Only XYZ direction changes. Native speed/W, angular velocity, animation,
reference counts,75AD20 fanout and downstream physics remain native. No EXE,
static table, character state or body array is modified by the implementation.
Existing character attacks/executions still use stock face-button actions.
This does not implement controller steering of a character while it is held.

The old6022144 actual-source fixture fails at its first character token
admission check (line86). The updated fixture passes316 checks, including the
recognizer-to-character release, target transitions, forged native reference,
retained-target refusal, one-shot consumption and native speed/W preservation.
A private copy of the actual92-byte75AD20 consumer executes against an owned
stub backend: two live bodies receive the same velocity pointer/mode2, the
empty body is skipped and native reference increments are balanced. No game
bytes enter Git.21 pinned instruction contracts and the consumer SHA256 pass.
The comparison ABI fixture passes2000 native-vs-adapter calls including the
exact ESI argument, GPR/EFLAGS/x87/SSE/MXCSR, LastError and stack/return state.
Aim461 and equipped-weapon74795 checks pass; both x86 DLL flavors build.

These isolated native-consumer tests do not execute the full enemy animation,
ragdoll or collision lifecycle. In-game direction, stability and the historical
enemy-lift crash remain unverified; no FPS or complete enemy-interaction claim.
