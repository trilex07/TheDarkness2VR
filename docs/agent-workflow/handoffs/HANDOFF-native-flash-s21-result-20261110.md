# S21: actual native Flash receipt SUCCESS, current rollback verified

## Actual outcome

MAIN granted one nativeFlash-only slot after independently accepting S20 cleanup.
Before acquisition: packet9A962 all28pins PASS, sidecar6CF5 and reviewDC1 all6
records PASS, current profile/inventory PASS, canonicalFREE/no-games.
No framework/source/guard edits in this slot. Exact Dot4C2A/current81FD-761F-
1172/A76 process-local, unchanged native639c.

Bootstrap used its emitted focus-expected.json. Focus succeeded only after
ordinary activation returnedfalse: two thread joins, successful attached
SetForegroundWindow, both joins detached in reverse, exact native foreground.
NativePID52956/HWND8390752; initial same-PID preview HWND36898852 was different.

Actual title1/both.png viewed in BOTH eyes/currentflags2. ONE released Enter
delivered, image_age13.909291744s, sequence1. Its immediate postimage remained
Title, not accepted as Main. Later main-pre/both.png was actually viewed: native
Main with Continue/NewGame/Vendettas/Options/Credits/Download/Exit in BOTH eyes,
currentflags1. All confirmations stopped; no second Title or Main action.

Immediately used the reviewed native_flash_once.py exactly once: return0,
READ_ONLY_EQUAL_PAIR, loaded37methods/19slots,160reads/28,204bytes. No local
image TTL, pasted qualification wrapper, retry, target call/hook/write. Helper
checks exact current token/PID/creation/path/HWND/foreground before/between/after
its TWO equal bounded snapshots. Its skips/lifetime/atomic qualifications stand.
Optional external client/clip was read ONLY after this success, then early close.

## Actual producer/consumer measurements

| Field | Actual S21 value |
|---|---|
|Flash manager viewport|3345 x 3410|
|top movie extents|1120 x 584|
|top movie viewport and copy|3345 x 3410|
|movie offsets|0,0|
|input scales|0.33482810854911804,0.1712609976530075|
|raw movie XY|761,1071|
|mouse XY/bounds|761,1071,1280,1400|
|external native client rect|0,0,1934,1071|
|window rect|-7,4,1933,1104|
|screen clip rect|0,30,1920,1080|
|client screen corners|-4,30 and1930,1101|
|DPI|96; helper PER_MONITOR_AWARE_V2|
|native foreground before/after external read|8390752, exact verified owner|

Current log: GazeMenu mode2 UV(.595,.871) -> desired(761,1219) of1280x1400;
native cursor(761,1071), acquired1. Source3345x3410. Phase history[2,1].
The native input bound fields really contain the mod's1280x1400 widening, yet
nativeY still saturates1071. This rules out treating widened fields alone as a
working fix and rules out assuming active Flash viewport equals client/eyeCache.

Verified6D56B0 consumer formula with the ACTUAL snapshot predicts pre-rounding
movie point(254.804190606,183.420528486):
x=(rawX-offsetX/2)*scaleX, y=(rawY+offsetY/2)*scaleY.
These are computed consumer values, NOT observed hover acceptance or a traced
final raster cursor. No claim that a UV*client or UV*source substitution is safe:
the projected quad's full-extent correspondence and native cursor-event/clamp
ownership must both be respected. No blind UV scaling or clamp relaxation.

Exact mod sites unchanged by Dot: eye_gaze.inl ControllerMenuHit selects
g_eyeCache cursor extent; ApplyGazeMenuCursor computes desiredXY in that extent
and widens mouse bounds. controller_menu_laser.inl separately renders the ray
from projected UV without reading actual clamped/native-scaled endpoint.
This is now a MEASURED coordinate-contract mismatch, not an assumed client-size
replacement. Native scales tie to selected render viewport3345x3410, not actual
client1934x1071 nor XR eye output1280x1400. A future isolated fix needs the
correct consumer/event boundary, not another backbuffer/client-only clamp patch.

No hover, click/selection, Dot row cycle, world, shot, bar, gesture/reload or
new Main confirm occurred. No gameplay/laser/horizon or headset acceptance.

## Clocks and identity

Token317b7eb8-4f61-473c-b72f-9a2e12dd19d0, ownerdarkness2.
A2026-11-10T16:21:52.246430Z; native before16:25:42.246430Z;
close16:27:42.246430Z/inputmax16:27:52.246430Z/cleanup16:29:52.246430Z.
Created ticks134388013148715619, session52956-289623500-1, lifecycle6012.
Main image created16:22:54.756914Z; native output completion filemtime
16:23:05.228160Z. These are separate times, NOT an atomic image/native frame.
External read16:23:28.1030632Z..16:23:28.1039427Z.

Owned normal WM_CLOSE16:23:28.235351Z; restore verified16:23:29.016748Z;
history release16:23:29.746174Z. No kill/manual recovery/close-race refusal.
Actual game exitcode UNKNOWN: existing backend had no retained wait handle.

## Independent fresh rollback

16:23:59.320392Z:14 current/backup SHA pairs PASS,1 originally absent path
still absent, EXE/loader match, normalized protected globalXR/Steam equal,
canonicalFREE/no-games. Observer SteamAppId/SteamGameId/XR_RUNTIME_JSON allnull;
not a claim of reading an exited Steam process environment. removed=[].
Current saves/config/log restored from THIS fresh session, no historical saves.

Exactly left installed:
- DLL81FDDEBA5F4C37C562DFC0F510E54365E9D2835C779BD026E3137969EE406723
- mainINI761F827A9FCDC998DA510EC344E20A6C84BC50187787DD13588E3C44D486AF60
- diagnostic117230AC717C2A5FE7C1CA82BF6A693B2A056855D4B47F18CCA941325462C076
User gesture/retry/elevation1 retained byte-exactly. Dot4C2A not retained.

## Evidence (private/ignored; no game-derived content committed)

Root captures/aim-indicator/native-flash-s21-20261110-162151:
- bootstrap/both.png,both.json,focus-expected.json; focus.json
- title1/both.png,dispatch.png,delivery.json,post.png/post.json
- main-pre/both.png,both.json,observation.json,phase-readiness.json
- native-once.json SHA5BF81B755300872B2FA6615091475E241A5BDD2C5448CF382AE00855639E3F9A
- external-window.json SHA9672F618027BF07C8790B7236639A7E2463E7DC4E8CABC7EDE5AA23459F7FAF9
- session/fresh/COMPLETE.json;after/03.bin runtime log
- session/cleanup-verified.json SHA1D0E04272C707C875F074A3AE39D466193B630A8DEE35FD8E8DB3BFBCE896121
- session/independent-cleanup-verified.json SHAD16600FEB9904430D8C9E89D1435D0C8007D83EBCEE19728B953792DDA23F147
- session/normal-close-request.json;release-status.json

Grant consumed, no automatic reacquire. Continue CPU coordinate-consumer fix
analysis on this actual evidence; product/frozen packet/sidecar unchanged.
