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
