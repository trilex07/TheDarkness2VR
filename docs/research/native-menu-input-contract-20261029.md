# Native input service: corrected initializer ABI

Static evidence only, D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1.
This resolves the specific initializer-argument uncertainty recorded in MODLOG;
it does not establish a live framework address or authorize native input writes.

9F4580 creates/adopts the input manager into its local reference at normal
ESP+14. The plain Windows path calls 63C950 at 9F48A5; other platform paths use
625670/8D89A0. Graphics and sound references occupy ESP+10/+18. At 9F4B01,
EDI is set to framework+A8; the client reference is assigned there at 9F4B1F.
At 9F4C55/57, ESI becomes the client object through that reference.

The misleading call at 9F4C6F has **no stack arguments and no stack cleanup**:
C32680 returns the service-global address 10DE3F8. Static initializer EA4FD0
calls C5CC40 for that object; its constructor sets vtable FBAC40. Slot +1C is
74B5F0, exactly `lea eax,[ecx+50h]; ret`. Three words pushed before this getter
are arguments for the subsequent C2C1A0 call, not getter arguments. No factory
or guessed calling convention is needed to resolve it.

C2C1A0 has client in ECX and seven stack arguments:

1. Framework object.
2. Address of service-global+50.
3. Address of local graphics reference (normal caller ESP+10).
4. Address of local sound reference (normal caller ESP+18).
5. Address of local input reference (normal caller ESP+14).
6. Framework+10 reference value, with its count incremented by the caller.
7. 1102080.

The initializer subtracts 2C and saves four registers. At its stable ESP,
reads +48/+4C/+50 name arguments 3/4/5. At C2C38C, C2C3A2 and C2C3B4,
7A8570 assigns the respective references to client+2C4 (sound), +2D0
(graphics), and +374 (input). Its RET 4 consumes the temporary argument each
time; C2C1A0's RET 1C consumes all seven original arguments. 7A8570 manages
intrusive reference counters: the stored field is not itself an InputMgrWin
object. No claim of exclusive ownership, thread safety or strong retention is
made from these offsets.

`python -B tools/native-camera/audit_input_service.py <installed EXE>` checks
the exact file SHA/PE identity and decisive constructor/getter/frame bytes.
It prints a static receipt and explicitly leaves live-object/type, menu state,
menu surface and injection admission false. The correct installed EXE passes;
a changed image is rejected. No game process, DLL, INI or Steam settings change.

Next: establish the existing framework's live ownership/lifetime, then verify
the input receiver type and cursor/event contract. The game's actual submitted
menu surface and pause/menu state are separate requirements; the already
implemented compositor settings quad cannot certify them.

## Performance priority decision

The retained zero-loss outdoor capture 20261027T141205Z-c4e8bb85 has inclusive
weights 54/87 for C6AFEE/C6B029 over 559 presents: approximately .252 nominal
sampled ms per present combined. These are different calls to the two material
dispatchers, not exclusive removable work or a current-HEAD FPS measurement.
425C70/9DCC90 select per-material/per-object parameter overrides and defaults;
there is no proof that these submissions are redundant. This path does not
justify another large reconstruction as the substantial CPU optimization.
Do not add its exposure to overlapping RecPush/RPM or module totals.

The larger WOW64 memset bucket has no native caller in its largest retained
mixed-architecture stack; its producer is the service dispatcher, already
documented in outdoor-cpu-packets-20261026.md. The recording arena already
avoids resize value-initialization. Neither observation supports removing
native resource work, disabling required synchronization, or enabling unsafe
DirectRecordCopy. Current accepted Steam performance/rendering code is intact.
